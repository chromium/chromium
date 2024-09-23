// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_EGTEST_UTIL_H_

#import <Foundation/Foundation.h>

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

@protocol GREYMatcher;

// Constants for Find in Page test content.
extern const char kFindInPageTestRepeatingText[];
extern const char kFindInPageTestShortTextID[];
extern const char kFindInPageTestShortText[];
extern const char kFindInPageTestLongText[];
extern const char kFindInPageTestSpecialCharactersText[];
extern const char kFindInPageTestNumbersText[];
extern const char kFindInPageTestAlphanumericText[];
extern const char kFindInPageTestNonASCIIText[];
extern const char kFindInPageTestWithSpanishAccentText[];
extern const char kFindInPageTestWithoutSpanishAccentText[];
extern const char kFindInPageTestLowercaseAndUppercaseText[];
extern const char kFindInPageTestRTLText[];

// Relative URLs for testing purposes.
extern const char kFindInPageTestURL[];
extern const char kFindInPageCrossOriginFrameTestURL[];
extern const char kFindInPageComplexPDFTestURL[];

// Returns Paste button matcher from UIMenuController.
id<GREYMatcher> PasteButton();

// Delegate for FindInPageTestCaseHelper. A test case can implement this
// protocol to define how to perform basic Find in Page operations, and then set
// itself as the delegate of this helper.
@protocol FindInPageTestCaseHelperDelegate

// Opens Find in Page.
- (void)openFindInPageWithOverflowMenu;
// Closes Find in page.
- (void)closeFindInPageWithDoneButton;
// Replaces the text in the Find in page textfield.
- (void)replaceFindInPageText:(NSString*)text;
// Paste text into Find in page textfield.
- (void)pasteTextToFindInPage:(NSString*)text;
// Clear text in Find in Page text field.
- (void)clearFindInPageText;
// Matcher for find in page textfield.
- (id<GREYMatcher>)findInPageInputField;
// Asserts that there is a string "`resultIndex` of `resultCount`" present in
// the results count label. Waits for up to 1 second for this to happen.
- (void)assertResultStringIsResult:(int)resultIndex outOfTotal:(int)resultCount;
// Asserts that there is a string "0 of 0" present in the results count label,
// or that the label is not visible. Waits for up to 1 second for this to
// happen.
- (void)assertResultStringIsEmptyOrZero;
// Asserts that there is a string in the results count label, that is not "0 of
// 0". Waits for up to 1 second for this to happen.
- (void)assertResultStringIsNonZero;
// Taps Next button in Find in page.
- (void)advanceToNextResult;
// Taps Previous button in Find in page.
- (void)advanceToPreviousResult;

@end

// Test helper for Native Find in Page. Many tests use the `secondTestServer` to
// ensure what is being tested also works with cross-origin iframes.
@interface FindInPageTestCaseHelper : NSObject

// Delegate.
@property(nonatomic, weak) id<FindInPageTestCaseHelperDelegate> delegate;

// First test server.
@property(nonatomic, assign) net::test_server::EmbeddedTestServer* testServer;

// Sets up two test servers to test Find in Page on web pages which might
// contain cross-origin iframes.
- (void)setUpTestServersForWebPageTest;
// Second test server so cross-origin iframes can be tested together with
// ChromeTestCase's `testServer`.
- (net::test_server::EmbeddedTestServer*)secondTestServer;
// Matcher similar to `grey_text` but more generic i.e. only looks at `hasText`
// prefix.
- (id<GREYMatcher>)matcherForText:(NSString*)text;

#pragma mark - Test scenarios

// Tests that FIP can be opened with Overflow menu.
- (void)helperTestFindInPageFromOverflowMenu;
// Tests that characters appear in the search box and that results UI updates as
// each characters is entered/deleted.
- (void)helperTestFindInPageTextInput;
// Tests that the number of results for a query accounts for all the matches
// across frames, here with a main frame and a cross-origin iframe.
- (void)helperTestFindInPageSupportsCrossOriginFrame;
// Tests that FIP can find different types of characters: special characters,
// number, strings with both letters and numbers as well as non-ASCII
// characters.
- (void)helperTestFindInPageSpecialCharacters;
// Tests that text can be copied from the web page and pasted into the FIP input
// field and that the results UI updates accordingly.
- (void)helperTestFindInPageCopyPaste;
// Tests that FIP yields no results for an empty search query.
- (void)helperTestFindInPageEmptySearchQuery;
// Tests that FIP yields no results for a non-empty query with no matches in the
// page.
- (void)helperTestFindInPageQueryWithNoMatches;
// Tests that FIP yields no matches for a text with spanish accents e.g. 'á' if
// the web page contains the same text without spanish accents e.g. 'a'. This
// test assumes removing accents from `kFindInPageTestWithSpanishAccentText`
// yields `kFindInPageTestWithoutSpanishAccentText`.
- (void)helperTestFindInPageDifferentAccent;
// Test that there is no query persistence with this variant of Native Find in
// Page i.e. with Find interaction.
- (void)helperTestFindInPageHistoryWithQueryPersistence:(BOOL)queryPersistence;
// Tests that there is no query persistence from an non-Incognito to an
// Incognito tab.
- (void)helperTestFindInPageNormalToIncognito;
// Tests that Next/Previous buttons work and wrap.
- (void)helperTestFindInPageNextPreviousArrows;
// Tests the various ways to dismiss the keyboard during a Find session.
- (void)helperTestFindInPageDismissKeyboard;
// Tests that FIP can find long strings of characters.
- (void)helperTestFindInPageLongString;
// Tests that FIP is not case sensitive.
- (void)helperTestFindInPageNotCaseSensitive;
// Tests that there is no leak of the FIP search query from Incognito tabs to
// normal tabs.
- (void)helperTestFindInPageIncognitoHistory;
// Tests that there is no query persistence when coming back to a normal tab
// after switching temporarily to another tab.
- (void)helperTestFindInPageSwitchingTabsWithQueryPersistence:
    (BOOL)queryPersistence;
// Tests that FIP can find RTL text in a web page.
- (void)helperTestFindInPageRTL;
// Tests that Find in Page can find matches in an Incognito tab.
- (void)helperTestFindInPageIncognito;
// Tests accessibility of the Find in Page screen.
- (void)helperTestFindInPageAccessibility;
// Tests that Native Find in Page works as expected for PDF documents.
- (void)helperTestFindInPagePDF;
// Tests that FIP exit fullscreen when done.
- (void)helperTestFindInPageExitFullscreen;
@end

#endif  // IOS_CHROME_BROWSER_FIND_IN_PAGE_MODEL_FIND_IN_PAGE_EGTEST_UTIL_H_
