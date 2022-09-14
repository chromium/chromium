// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using chrome_test_util::AutofillSuggestionViewMatcher;
using chrome_test_util::ManualFallbackFormSuggestionViewMatcher;
using chrome_test_util::ManualFallbackKeyboardIconMatcher;
using chrome_test_util::ManualFallbackManageProfilesMatcher;
using chrome_test_util::ManualFallbackProfilesIconMatcher;
using chrome_test_util::ManualFallbackProfilesTableViewMatcher;
using chrome_test_util::ManualFallbackProfileTableViewWindowMatcher;

namespace {

constexpr char kFormElementName[] = "name";
constexpr char kFormElementCity[] = "city";

constexpr char kFormHTMLFile[] = "/profile_form.html";

// Returns a matcher for a button in the ProfileTableView. Currently it returns
// the company one.
id<GREYMatcher> ProfileTableViewButtonMatcher() {
  // The company name for autofill::test::GetFullProfile() is "Underworld".
  return grey_buttonTitle(@"Underworld");
}

}  // namespace

// Integration Tests for fallback coordinator.
@interface FallbackCoordinatorTestCase : ChromeTestCase

@end

@implementation FallbackCoordinatorTestCase

- (void)setUp {
  [super setUp];
  // If the previous run was manually stopped then the profile will be in the
  // store and the test will fail. We clean it here for those cases.
  [AutofillAppInterface clearProfilesStore];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];
}

- (void)tearDown {
  [AutofillAppInterface clearProfilesStore];

  // Leaving a picker on iPads causes problems with the docking logic. This
  // will dismiss any.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap in the web view so the popover dismisses.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:grey_tapAtPoint(CGPointMake(0, 0))];

    // Verify the table view is not visible.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_kindOfClass([UITableView class]),
                                     grey_not(grey_notVisible()), nil)]
        assertWithMatcher:grey_nil()];
  }
  [super tearDown];
}

// Tests that the when tapping the outside the popover on iPad, suggestions
// continue working.
- (void)testIPadTappingOutsidePopOverResumesSuggestionsCorrectly {
  if (![ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Test not applicable for iPhone.");
  }

  // Add the profile to be tested.
  [AutofillAppInterface saveExampleProfile];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementName)];

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:ManualFallbackProfileTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the profiles controller table view is NOT visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Tap on the suggestion.
  [[EarlGrey selectElementWithMatcher:AutofillSuggestionViewMatcher()]
      performAction:grey_tap()];

  // Verify Web Content was filled.
  NSString* name = [AutofillAppInterface exampleProfileName];
  NSString* javaScriptCondition = [NSString
      stringWithFormat:@"document.getElementById('%s').value === '%@'",
                       kFormElementName, name];
  [ChromeEarlGrey waitForJavaScriptCondition:javaScriptCondition];
}

// Tests that the manual fallback view concedes preference to the system picker
// for selection elements.
- (void)testPickerDismissesManualFallback {
  // Add the profile to be used.
  [AutofillAppInterface saveExampleProfile];

  // Bring up the keyboard.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // Verify icons are not present now that the selected field is a picker.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_notVisible()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that the input accessory view continues working after a picker is
// present.
- (void)testInputAccessoryBarIsPresentAfterPickers {
  // Add the profile to be used.
  [AutofillAppInterface saveExampleProfile];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Tap on the profiles icon.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesTableViewMatcher()]
      assertWithMatcher:grey_notVisible()];

  // On iPad and iOS 15 the picker is a table view in a popover, we need to
  // dismiss that first.
  if ([ChromeEarlGrey isIPadIdiom] || base::ios::IsRunningOnIOS15OrLater()) {
    // Tap in the web view so the popover dismisses.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:grey_tapAtPoint(CGPointMake(0, 0))];

    // Verify the table view is not visible.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_kindOfClass([UITableView class]),
                                     grey_not(grey_notVisible()), nil)]
        assertWithMatcher:grey_nil()];

    // Dismissing the popover by tapping on the webView, then tapping on the
    // form element below in quick succession seems to end up dismissing the
    // keyboard on iOS15. This may be because the state element is still
    // focused. Instead, wait a moment for the focus to be dismissed.
    if (base::ios::IsRunningOnIOS15OrLater()) {
      base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));
    }
  }

  // Bring up the regular keyboard again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible, and therefore also the input accessory
  // bar.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  // Verify the status of the icons.
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_userInteractionEnabled()];
  [[EarlGrey selectElementWithMatcher:ManualFallbackKeyboardIconMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that the manual fallback view is present in incognito.
- (void)testIncognitoManualFallbackMenu {
  // Add the profile to use for verification.
  [AutofillAppInterface saveExampleProfile];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass(
                                          NSClassFromString(@"WKWebView"))]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the mediator stops observing objects when the incognito BVC is
// destroyed. Waiting for dealloc was causing a race condition with the
// autorelease pool, and some times a DCHECK will be hit.
// TODO(crbug.com/1227124) Flaky test.
- (void)DISABLED_testOpeningIncognitoTabsDoNotLeak {
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  std::string webViewText("Profile form");
  [AutofillAppInterface saveExampleProfile];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Open a  regular tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // This will fail if there is more than one profiles icon in the hierarchy.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view is not duplicated after incognito.
// TODO(crbug.com/1228283): Disabled due to flakiness.
- (void)DISABLED_testReturningFromIncognitoDoesNotDuplicatesManualFallbackMenu {
  // Add the profile to use for verification.
  [AutofillAppInterface saveExampleProfile];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // Open a  regular tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(kFormElementCity)];

  // This will fail if there is more than one profiles icon in the hierarchy.
  [[EarlGrey selectElementWithMatcher:ManualFallbackFormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:ManualFallbackProfilesIconMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
