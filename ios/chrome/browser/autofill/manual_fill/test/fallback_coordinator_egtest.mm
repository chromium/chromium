// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/ios_util.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/autofill/manual_fill/test/manual_fill_matchers.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_app_interface.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_egtest_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

using autofill::FocusWebElementWithId;
using chrome_test_util::AutofillSuggestionViewMatcher;

namespace {

constexpr char kFormElementName[] = "name";
constexpr char kFormElementCity[] = "city";

constexpr char kFormHTMLFile[] = "/profile_form.html";

// Matcher for the keyboard accessory's expand button.
id<GREYMatcher> KeyboardAccessoryExpandButton() {
  return grey_accessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_ACCNAME_AUTOFILL_DATA));
}

// Returns a matcher for a button in the ProfileTableView. Currently it returns
// the City one.
id<GREYMatcher> ProfileTableViewButtonMatcher() {
  // The city name for autofill::test::GetFullProfile() is "Elysium".
  return grey_buttonTitle(@"Elysium");
}

// Opens the address manual fill view and verifies that the address view
// controller is visible afterwards.
void OpenAddressManualFillView() {
  // Tap the button that'll open the address manual fill view.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      performAction:grey_tap()];

  // Verify the address table view controller is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
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

- (void)tearDownHelper {
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
  [super tearDownHelper];
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

  // Open the address manual fill view and verify that the address table view
  // controller is visible.
  OpenAddressManualFillView();

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey
      selectElementWithMatcher:manual_fill::ProfileTableViewWindowMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the profiles controller table view is NOT visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::ProfilesTableViewMatcher()]
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

  // Open the address manual fill view and verify that the address table view
  // controller is visible.
  OpenAddressManualFillView();

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [ChromeEarlGrey waitForNotSufficientlyVisibleElementWithMatcher:
                      manual_fill::ProfilesTableViewMatcher()];

  // Verify icon is not present now that the selected field is a picker.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the input accessory view continues working after a picker is
// present.
- (void)testInputAccessoryBarIsPresentAfterPickers {
  // Add the profile to be used.
  [AutofillAppInterface saveExampleProfile];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  FocusWebElementWithId(kFormElementCity);

  // Open the address manual fill view and verify that the address table view
  // controller is visible.
  OpenAddressManualFillView();

  // Tap any option.
  [[EarlGrey selectElementWithMatcher:ProfileTableViewButtonMatcher()]
      performAction:grey_tap()];

  // Verify the profiles controller table view is not visible.
  [ChromeEarlGrey waitForNotSufficientlyVisibleElementWithMatcher:
                      manual_fill::ProfilesTableViewMatcher()];

  // Tap in the web view so the picker dismisses.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // On iPad the picker is a table view in a popover, verify it is dismissed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_kindOfClass([UITableView class]),
                                     grey_not(grey_notVisible()), nil)]
        assertWithMatcher:grey_nil()];
  }

  // Bring up the regular keyboard again. `FocusWebElementWithId()` retries
  // tapping `kFormElementCity` until it acquires DOM focus, avoiding the race
  // where a single tap immediately after dismissing the `state` picker fails to
  // focus the field.
  FocusWebElementWithId(kFormElementCity);

  // Verify the manual fill icon is visible, and therefore also the input
  // accessory bar.
  [ChromeEarlGrey
      waitForUIElementToAppearWithMatcher:KeyboardAccessoryExpandButton()];

  // Verify the status of the icon.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_userInteractionEnabled()];
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

  // Verify that the manual fill button is visible.
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the mediator stops observing objects when the incognito BVC is
// destroyed. Waiting for dealloc was causing a race condition with the
// autorelease pool, and some times a DCHECK will be hit.
// TODO(crbug.com/40776865): Test fails on Simulator.
#if TARGET_OS_SIMULATOR
#define MAYBE_testOpeningIncognitoTabsDoNotLeak \
  DISABLED_testOpeningIncognitoTabsDoNotLeak
#else
#define MAYBE_testOpeningIncognitoTabsDoNotLeak \
  testOpeningIncognitoTabsDoNotLeak
#endif  // TARGET_OS_SIMULATOR
- (void)MAYBE_testOpeningIncognitoTabsDoNotLeak {
  [AutofillAppInterface saveExampleProfile];

  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  std::string webViewText("Profile form");

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified([ElementSelector
                        selectorWithElementID:kFormElementCity])];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified([ElementSelector
                        selectorWithElementID:kFormElementCity])];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified([ElementSelector
                        selectorWithElementID:kFormElementCity])];

  // Verify the profiles icon is visible.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified([ElementSelector
                        selectorWithElementID:kFormElementCity])];

  // Open a  regular tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:webViewText];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementUnverified([ElementSelector
                        selectorWithElementID:kFormElementCity])];

  // This will fail if there is more than one profiles icon in the hierarchy.
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the manual fallback view is not duplicated after incognito.
- (void)testReturningFromIncognitoDoesNotDuplicateManualFallbackMenu {
  // Add the profile to use for verification.
  [AutofillAppInterface saveExampleProfile];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  FocusWebElementWithId(kFormElementCity);

  // Verify the profiles icon is visible.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::FormSuggestionViewMatcher()];
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open a tab in incognito.
  [ChromeEarlGrey openNewIncognitoTab];
  const GURL URL = self.testServer->GetURL(kFormHTMLFile);
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  FocusWebElementWithId(kFormElementCity);
  // Make sure the incognito keyboard accessory was actually created before
  // switching back to a regular tab, otherwise the duplication scenario this
  // test covers would not be exercised.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::FormSuggestionViewMatcher()];

  // Open a  regular tab.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGrey waitForWebStateContainingText:"Profile form"];

  // Bring up the keyboard by tapping the city, which is the element before the
  // picker.
  FocusWebElementWithId(kFormElementCity);

  // This will fail if there is more than one profiles icon in the hierarchy.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      manual_fill::FormSuggestionViewMatcher()];
  [[EarlGrey selectElementWithMatcher:manual_fill::FormSuggestionViewMatcher()]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeRight)];
  [[EarlGrey selectElementWithMatcher:KeyboardAccessoryExpandButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
