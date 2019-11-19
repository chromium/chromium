// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/feature_flags.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/util/transparent_link_button.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ContextMenuCopyButton;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::OpenLinkInNewTabButton;

namespace {
char kURL1[] = "http://firstURL";
char kURL2[] = "http://secondURL";
char kURL3[] = "http://thirdURL";
char kTitle1[] = "Page 1";
char kTitle2[] = "Page 2";
char kResponse1[] = "Test Page 1 content";
char kResponse2[] = "Test Page 2 content";
char kResponse3[] = "Test Page 3 content";

// Matcher for entry in history for URL and title.
id<GREYMatcher> HistoryEntry(const GURL& url, const std::string& title) {
  NSString* url_spec_text = nil;
  NSString* title_text = base::SysUTF8ToNSString(title);

    url_spec_text = base::SysUTF8ToNSString(url.GetOrigin().spec());

    MatchesBlock matches = ^BOOL(TableViewURLCell* cell) {
      return [cell.titleLabel.text isEqual:title_text] &&
             [cell.URLLabel.text isEqual:url_spec_text];
    };

    DescribeToBlock describe = ^(id<GREYDescription> description) {
      [description appendText:@"view containing URL text: "];
      [description appendText:url_spec_text];
      [description appendText:@" title text: "];
      [description appendText:title_text];
    };
    return grey_allOf(
        grey_kindOfClass([TableViewURLCell class]),
        [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                             descriptionBlock:describe],
        grey_sufficientlyVisible(), nil);
}
// Matcher for the history button in the tools menu.
id<GREYMatcher> HistoryButton() {
  return grey_accessibilityID(kToolsMenuHistoryId);
}
// Matcher for the edit button in the navigation bar.
id<GREYMatcher> NavigationEditButton() {
  return grey_accessibilityID(kHistoryToolbarEditButtonIdentifier);
}
// Matcher for the delete button.
id<GREYMatcher> DeleteHistoryEntriesButton() {
  return grey_accessibilityID(kHistoryToolbarDeleteButtonIdentifier);
}
// Matcher for the search button.
id<GREYMatcher> SearchIconButton() {
    return grey_accessibilityID(kHistorySearchControllerSearchBarIdentifier);
}
// Matcher for the cancel button.
id<GREYMatcher> CancelButton() {
  return grey_accessibilityID(kHistoryToolbarCancelButtonIdentifier);
}
// Matcher for the Open in New Incognito Tab option in the context menu.
id<GREYMatcher> OpenInNewIncognitoTabButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB);
}

}  // namespace

// History UI tests.
@interface HistoryUITestCase : ChromeTestCase {
  GURL _URL1;
  GURL _URL2;
  GURL _URL3;
}

// Loads three test URLs.
- (void)loadTestURLs;
// Displays the history UI.
- (void)openHistoryPanel;
// Resets which data is selected in the Clear Browsing Data UI.
- (void)resetBrowsingDataPrefs;

@end

@implementation HistoryUITestCase

// Set up called once for the class.
+ (void)setUp {
  [super setUp];
  std::map<GURL, std::string> responses;
  const char kPageFormat[] = "<head><title>%s</title></head><body>%s</body>";
  responses[web::test::HttpServer::MakeUrl(kURL1)] =
      base::StringPrintf(kPageFormat, kTitle1, kResponse1);
  responses[web::test::HttpServer::MakeUrl(kURL2)] =
      base::StringPrintf(kPageFormat, kTitle2, kResponse2);
  // Page 3 does not have <title> tag, so URL will be its title.
  responses[web::test::HttpServer::MakeUrl(kURL3)] = kResponse3;
  web::test::SetUpSimpleHttpServer(responses);
}

- (void)setUp {
  [super setUp];
  _URL1 = web::test::HttpServer::MakeUrl(kURL1);
  _URL2 = web::test::HttpServer::MakeUrl(kURL2);
  _URL3 = web::test::HttpServer::MakeUrl(kURL3);
  [ChromeEarlGrey clearBrowsingHistory];
  // Some tests rely on a clean state for the "Clear Browsing Data" settings
  // screen.
  [self resetBrowsingDataPrefs];
}

- (void)tearDown {
  NSError* error = nil;
  // Dismiss search bar by pressing cancel, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:CancelButton()] performAction:grey_tap()
                                                              error:&error];
  // Dismiss history panel by pressing done, if present. Passing error prevents
  // failure if the element is not found.
  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()
              error:&error];

  // Some tests change the default values for the "Clear Browsing Data" settings
  // screen.
  [self resetBrowsingDataPrefs];
  [super tearDown];
}

#pragma mark Tests

// Tests that no history is shown if there has been no navigation.
- (void)testDisplayNoHistory {
  [self openHistoryPanel];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

// Tests that the history panel displays navigation history.
- (void)testDisplayHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that history displays three entries.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Tap a history entry and assert that navigation to that entry's URL occurs.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];
}

// Tests that history is not changed after performing back navigation.
- (void)testHistoryUpdateAfterBackNavigation {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey loadURL:_URL2];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [self openHistoryPanel];

  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
}

// Tests that searching history displays only entries matching the search term.
- (void)testSearchHistory {
  // TODO(crbug.com/753098): Re-enable this test on iPad once grey_typeText
  // works on iOS 11.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iPad.");
  }

  [self loadTestURLs];
  [self openHistoryPanel];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

    // Verify that scrim is visible.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kHistorySearchScrimIdentifier)]
        assertWithMatcher:grey_notNil()];

  NSString* searchString =
      [NSString stringWithFormat:@"%s", _URL1.path().c_str()];

  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(searchString)];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_nil()];
}

// Tests that long press on scrim while search box is enabled dismisses the
// search controller.
- (void)testSearchLongPressOnScrimCancelsSearchController {
  [self loadTestURLs];
  [self openHistoryPanel];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];

  // Try long press.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Verify context menu is not visible.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      assertWithMatcher:grey_nil()];

  // Verify that scrim is not visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistorySearchScrimIdentifier)]
      assertWithMatcher:grey_nil()];

  // Verifiy we went back to original folder content.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests deletion of history entries.
- (void)testDeleteHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Assert that three history elements are present.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select a history element, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  // Assert that the deleted entry is gone and the other two remain.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Enter edit mode, select both remaining entries, and press delete.
  [[EarlGrey selectElementWithMatcher:NavigationEditButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL2, kTitle2)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL3, _URL3.GetContent())]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:DeleteHistoryEntriesButton()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

// Tests clear browsing history.
- (void)testClearBrowsingHistory {
  [self loadTestURLs];
  [self openHistoryPanel];

  [ChromeEarlGreyUI openAndClearBrowsingDataFromHistory];
  [ChromeEarlGreyUI assertHistoryHasNoEntries];
}

// Tests clear browsing history.
- (void)testClearBrowsingHistorySwipeDownDismiss {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (!IsCollectionsCardPresentationStyleEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }

  [self loadTestURLs];
  [self openHistoryPanel];

  // Open Clear Browsing Data
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Check that the TableView is presented.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests display and selection of 'Open in New Tab' in a context menu on a
// history entry.
- (void)testContextMenuOpenInNewTab {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Select "Open in New Tab" and confirm that new tab is opened with selected
  // URL.
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          _URL1.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests display and selection of 'Open in New Incognito Tab' in a context menu
// on a history entry.
- (void)testContextMenuOpenInNewIncognitoTab {
  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Select "Open in New Incognito Tab" and confirm that new tab is opened in
  // incognito with the selected URL.
  [[EarlGrey selectElementWithMatcher:OpenInNewIncognitoTabButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          _URL1.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Tests display and selection of 'Copy URL' in a context menu on a history
// entry.
- (void)testContextMenuCopy {
  ProceduralBlock clearPasteboard = ^{
    [[UIPasteboard generalPasteboard] setURLs:nil];
  };
  [self setTearDownHandler:clearPasteboard];
  clearPasteboard();

  [self loadTestURLs];
  [self openHistoryPanel];

  // Long press on the history element.
  [[EarlGrey selectElementWithMatcher:HistoryEntry(_URL1, kTitle1)]
      performAction:grey_longPress()];

  // Tap "Copy URL" and wait for the URL to be copied to the pasteboard.
  [[EarlGrey selectElementWithMatcher:ContextMenuCopyButton()]
      performAction:grey_tap()];
  bool success = base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^{
        return _URL1 ==
               net::GURLWithNSURL([UIPasteboard generalPasteboard].URL);
      });
  GREYAssertTrue(success, @"Pasteboard URL was not set to %s",
                 _URL1.spec().c_str());
}

// Tests that the VC can be dismissed by swiping down.
- (void)testSwipeDownDismiss {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (!IsCollectionsCardPresentationStyleEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }
  [self loadTestURLs];
  [self openHistoryPanel];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Tests that the VC can be dismissed by swiping down while its searching.
- (void)testSwipeDownDismissWhileSearching {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  if (!IsCollectionsCardPresentationStyleEnabled()) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on when feature flag is off.");
  }
  [self loadTestURLs];
  [self openHistoryPanel];

  // Check that the TableView is presented.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_notNil()];

  // Search for the first URL.
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_tap()];
  NSString* searchString =
      [NSString stringWithFormat:@"%s", _URL1.path().c_str()];
  [[EarlGrey selectElementWithMatcher:SearchIconButton()]
      performAction:grey_typeText(searchString)];

  // Swipe TableView down.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

  // Check that the TableView has been dismissed.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kHistoryTableViewIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Navigates to history and checks elements for accessibility.
- (void)testAccessibilityOnHistory {
  [self loadTestURLs];
  [self openHistoryPanel];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  // Close history.
    id<GREYMatcher> exitMatcher =
        grey_accessibilityID(kHistoryNavigationControllerDoneButtonIdentifier);
    [[EarlGrey selectElementWithMatcher:exitMatcher] performAction:grey_tap()];
}

#pragma mark Helper Methods

- (void)loadTestURLs {
  [ChromeEarlGrey loadURL:_URL1];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse1];

  [ChromeEarlGrey loadURL:_URL2];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse2];

  [ChromeEarlGrey loadURL:_URL3];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse3];
}

- (void)openHistoryPanel {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:HistoryButton()];
}

- (void)resetBrowsingDataPrefs {
  PrefService* prefs = chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefs->ClearPref(browsing_data::prefs::kDeleteBrowsingHistory);
  prefs->ClearPref(browsing_data::prefs::kDeleteCookies);
  prefs->ClearPref(browsing_data::prefs::kDeleteCache);
  prefs->ClearPref(browsing_data::prefs::kDeletePasswords);
  prefs->ClearPref(browsing_data::prefs::kDeleteFormData);
}

@end
