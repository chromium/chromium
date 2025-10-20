// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <map>
#import <tuple>

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"
#import "ios/chrome/browser/browser_view/test/browser_view_visibility_app_interface.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/new_tab_page_app_interface.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings_app_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Urls and their content.
const char kFirstURL[] = "/pony.html";
const char kFirstURLText[] = "Anyone know any good pony jokes?";
const char kSecondURL[] = "/destination.html";
const char kSecondURLText[] = "You've arrived";

}  // namespace

// This test suite only tests javascript in the omnibox. Nothing to do with BVC
// really, the name is a bit misleading.
@interface BrowserViewControllerTestCase : ChromeTestCase
@end

@implementation BrowserViewControllerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  if ([self isRunningTest:@selector
            (testOpenSearchWidgetWithSignOutProfileSwitch)] ||
      [self isRunningTest:@selector
            (testOpenSearchWidgetWithSignInProfileSwitch)] ||
      [self isRunningTest:@selector
            (testOpenSearchWidgetWithUnmanagedToManagedProfileSwitch)]) {
    config.features_enabled.push_back(kSeparateProfilesForManagedAccounts);
  }

  return config;
}

// Tests that the NTP is interactable even when multiple NTP are opened during
// the animation of the first NTP opening. See crbug.com/1032544.
- (void)testPageInteractable {
  // Put MVT as the top magic stack module for easier tapping.
  [NewTabPageAppInterface disableTipsCards];

  // Ensures that the first favicon in Most Visited row is the test URL.
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kFirstURL)];

  // Scope for the synchronization disabled.
  {
    ScopedSynchronizationDisabler syncDisabler;

    [ChromeEarlGrey openNewTab];

    // Wait for 0.05s before opening the new one.
    GREYCondition* myCondition = [GREYCondition conditionWithName:@"Wait block"
                                                            block:^BOOL {
                                                              return NO;
                                                            }];
    std::ignore = [myCondition waitWithTimeout:0.05];

    [ChromeEarlGrey openNewTab];
  }  // End of the sync disabler scope.

  [ChromeEarlGrey waitForMainTabCount:3];

  NSInteger faviconIndex = 0;
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([NSString
              stringWithFormat:
                  @"%@%li",
                  kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                  faviconIndex])] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kFirstURLText];

  [ChromeEarlGrey selectTabAtIndex:1];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([NSString
              stringWithFormat:
                  @"%@%li",
                  kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
                  faviconIndex])] performAction:grey_tap()];
  [ChromeEarlGrey waitForWebStateContainingText:kFirstURLText];
  [NewTabPageAppInterface resetSetUpListPrefs];
}

// Tests that evaluating JavaScript in the omnibox (e.g, a bookmarklet) works.
// TODO(crbug.com/362621166): Test is flaky.
- (void)DIABLED_testJavaScriptInOmnibox {
  // TODO(crbug.com/40511873): Keyboard entry inside the omnibox fails only on
  // iPad running iOS 10.
  if ([ChromeEarlGrey isIPadIdiom]) {
    return;
  }

  // Preps the http server with two URLs serving content.
  std::map<GURL, std::string> responses;
  const GURL firstURL = self.testServer->GetURL(kFirstURL);
  const GURL secondURL = self.testServer->GetURL(kSecondURL);

  // Just load the first URL.
  [ChromeEarlGrey loadURL:firstURL];

  // Waits for the page to load and check it is the expected content.
  [ChromeEarlGrey waitForWebStateContainingText:kFirstURLText];

  // In the omnibox, the URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(firstURL.GetContent())];

  // Types some javascript in the omnibox to trigger a navigation.
  NSString* script =
      [NSString stringWithFormat:@"javascript:location.href='%s'",
                                 secondURL.spec().c_str()];
  [ChromeEarlGreyUI focusOmniboxAndReplaceText:script];

  // The omnibox popup may update multiple times.
  base::test::ios::SpinRunLoopWithMinDelay(base::Seconds(1));

  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];

  // In the omnibox, the new URL should be present, without the http:// prefix.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(secondURL.GetContent())];

  // Verifies that the navigation to the destination page happened.
  GREYAssertEqual(secondURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Did not navigate to the destination url.");

  // Verifies that the destination page is shown.
  [ChromeEarlGrey waitForWebStateContainingText:kSecondURLText];
}

// Tests the fix for the regression reported in https://crbug.com/801165.  The
// bug was triggered by opening an HTML file picker and then dismissing it.
- (void)testFixForCrbug801165 {
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no action sheet on tablet)");
  }

  const GURL testURL(base::StringPrintf(
      "data:text/html, File Picker Test <input id=\"file\" type=\"file\">"));
  // Load the test page.
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForWebStateContainingText:"File Picker Test"];

  // Invoke the file picker.
  [ChromeEarlGrey tapWebStateElementWithID:@"file"];

  // Tap on the toolbar to dismiss the file picker on iOS14.  In iOS14 a
  // UIDropShadowView covers the entire app, so tapping anywhere should
  // dismiss the file picker.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::PrimaryToolbar()]
      performAction:grey_tap()];

  [ChromeEarlGreyUI waitForAppToIdle];
}

// Tests that browser visibility state updates correctly.
- (void)testBrowserVisibilityState {
  // Go to a web page first to make sure the omnibox popup would have content.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kFirstURL)];

  // Starts observing visibility state.
  [BrowserViewVisibilityAppInterface startObservingBrowserViewVisibilityState];

  // Taps omnibox and checks visibility.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  GREYAssertEqual(BrowserViewVisibilityState::kCoveredByOmniboxPopup,
                  [BrowserViewVisibilityAppInterface currentState],
                  @"Browser visibility state is not accurate.");

  // Cancels omnibox and check visibility.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Defocus omnibox by tapping the typing shield.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
        performAction:grey_tap()];

  } else {
    id<GREYMatcher> cancel_button = grey_allOf(
        grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier),
        grey_sufficientlyVisible(), nil);
    [[EarlGrey selectElementWithMatcher:cancel_button]
        performAction:grey_tap()];
  }
  GREYAssertEqual(BrowserViewVisibilityState::kVisible,
                  [BrowserViewVisibilityAppInterface currentState],
                  @"Browser visibility state is not accurate.");

  // Goes to the tab grid and check visibility.
  [ChromeEarlGreyUI openTabGrid];
  GREYAssertEqual(BrowserViewVisibilityState::kNotInViewHierarchy,
                  [BrowserViewVisibilityAppInterface currentState],
                  @"Browser visibility state is not accurate.");

  // TODO(crbug.com/406544789): Check settings, promos, and lens.
  [BrowserViewVisibilityAppInterface stopObservingBrowserViewVisibilityState];
}

#pragma mark - Open URL

// Tests that BVC properly handles open URL. When NTP is visible, the URL
// should be opened in the same tab (not create a new tab).
- (void)testOpenURLFromNTP {
  [ChromeEarlGrey sceneOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that BVC properly handles open URL. When BVC is showing a non-NTP
// tab, the URL should be opened in a new tab, adding to the tab count.
- (void)testOpenURLFromTab {
  [ChromeEarlGrey loadURL:GURL("https://invalid")];
  [ChromeEarlGrey sceneOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that BVC properly handles open URL. When tab switcher is showing,
// the URL should be opened in a new tab, and BVC should be shown.
- (void)testOpenURLFromTabSwitcher {
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:0];
  [ChromeEarlGrey sceneOpenURL:GURL("https://anything")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          "https://anything")]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

#pragma mark - Widgets

// Tests that the Search Widget URL loads the NTP with the Omnibox focused.
- (void)testOpenSearchWidget {
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/search")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
}

#if BUILDFLAG(ENABLE_WIDGETS_FOR_MIM)
// Test that code for opening URLs from Search widgets loads the NTP with the
// Omnibox focused and switches to the correct account (in the same profile).
- (void)testOpenSearchWidgetWithoutProfileSwitch {
  FakeSystemIdentity* fakeIdentity1 = [FakeSystemIdentity fakeIdentity1];
  FakeSystemIdentity* fakeIdentity2 = [FakeSystemIdentity fakeIdentity2];

  // Test sign-out from unmanaged identity.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity1];
  [ChromeEarlGrey
      sceneOpenURL:
          GURL("chromewidgetkit://search-widget/search?gaia_id=No account")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  GREYAssertTrue([SigninEarlGrey isSignedOut], @"Failed to sign-out.");

  // Test sign-in to unmanaged identity.
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/"
                                    "search?gaia_id=foo1_gmail.com_GAIAID")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];

  // Test switch account in the same profile.
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity2];
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/"
                                    "search?gaia_id=foo1_gmail.com_GAIAID")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity1];
}

// Test that code for opening URLs from Search widgets loads the NTP with the
// Omnibox focused and switches to the correct profile and account.
- (void)testOpenSearchWidgetWithSignOutProfileSwitch {
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];

  // Test sign-out from managed account.
  [SigninEarlGrey
      signinWithFakeManagedIdentityInPersonalProfile:fakeManagedIdentity];
  [ChromeEarlGrey
      sceneOpenURL:
          GURL("chromewidgetkit://search-widget/search?gaia_id=No account")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  GREYAssertTrue([SigninEarlGrey isSignedOut], @"Failed to sign-out.");
}

// Test that code for opening URLs from Search widgets loads the NTP with the
// Omnibox focused and switches to the correct profile and account.
- (void)testOpenSearchWidgetWithSignInProfileSwitch {
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];

  // Test sign-in to managed account.
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/"
                                    "search?gaia_id=foo_google.com_GAIAID")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];
}

// Test that code for opening URLs from Search widgets loads the NTP with the
// Omnibox focused and switches to the correct profile and account.
- (void)testOpenSearchWidgetWithUnmanagedToManagedProfileSwitch {
  FakeSystemIdentity* fakeManagedIdentity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentity* fakeIdentity = [FakeSystemIdentity fakeIdentity1];

  [SigninEarlGrey addFakeIdentity:fakeManagedIdentity];
  [SigninEarlGrey signinWithFakeIdentity:fakeIdentity];

  // Test sign-in from unmanaged to managed account.
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://search-widget/"
                                    "search?gaia_id=foo_google.com_GAIAID")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeManagedIdentity];
}

// Test that code for opening URLs from Dino Widget opens the dino game and
// switches to the correct account.
- (void)testOpenDinoWidgetForMultiprofile {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      sceneOpenURL:
          GURL("chromewidgetkit://dino-game-widget/game?gaia_id=No account")];

  // The dino game should be loaded.
  [ChromeEarlGrey waitForPageToFinishLoading];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            base::SysNSStringToUTF8(@"chrome://dino"))];

  GREYAssertTrue([SigninEarlGrey isSignedOut], @"Failed to sign-out.");

  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://dino-game-widget/"
                                    "game?gaia_id=foo1_gmail.com_GAIAID")];
  GREYAssertTrue(![SigninEarlGrey isSignedOut], @"Failed to sign-in.");
}

// Test that code for opening URLs from Quick Actions Widget opens the app in
// incognito and switches to the correct account.
- (void)testOpenQuickActionsWidgetForMultiprofile {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://quick-actions-widget/"
                                    "incognito?gaia_id=No account")];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  // Verify the current tab is an incognito tab.
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Failed to switch to incognito mode");
  GREYAssertTrue([SigninEarlGrey isSignedOut], @"Failed to sign-out.");

  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://quick-actions-widget/"
                                    "incognito?gaia_id=foo1_gmail.com_GAIAID")];
  GREYAssertTrue(![SigninEarlGrey isSignedOut], @"Failed to sign-in.");
}

// Test that code for opening URLs from Shortcuts Widget correctly opens and
// switches to the correct account.
- (void)testOpenShortcutsWidgetForMultiprofile {
  [SigninEarlGrey signinWithFakeIdentity:[FakeSystemIdentity fakeIdentity1]];
  [ChromeEarlGrey
      sceneOpenURL:
          GURL("chromewidgetkit://shortcuts-widget/search?gaia_id=No account")];

  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  GREYAssertTrue([SigninEarlGrey isSignedOut], @"Failed to sign-out.");

  [ChromeEarlGrey sceneOpenURL:GURL("chromewidgetkit://shortcuts-widget/"
                                    "search?gaia_id=foo1_gmail.com_GAIAID")];
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:chrome_test_util::Omnibox()];
  GREYAssertTrue(![SigninEarlGrey isSignedOut], @"Failed to sign-in.");
}

#endif

#pragma mark - Multiwindow

// TODO(crbug.com/40240588): Re-enable this test.
- (void)DISABLED_testMultiWindowURLLoading {
  if (![ChromeEarlGrey areMultipleWindowsSupported]) {
    EARL_GREY_TEST_DISABLED(@"Multiple windows can't be opened.");
  }

  // Preps the http server with two URLs serving content.
  const GURL firstURL = self.testServer->GetURL(kFirstURL);
  const GURL secondURL = self.testServer->GetURL(kSecondURL);

  // Loads url in first window.
  [ChromeEarlGrey loadURL:firstURL inWindowWithNumber:0];

  // Opens second window and loads url.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];
  [ChromeEarlGrey loadURL:secondURL inWindowWithNumber:1];

  // Checks loads worked.
  [ChromeEarlGrey waitForWebStateContainingText:kFirstURLText
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kSecondURLText
                             inWindowWithNumber:1];

  // Closes first window and renumbers second window as first.
  [ChromeEarlGrey closeWindowWithNumber:0];
  [ChromeEarlGrey waitForForegroundWindowCount:1];
  [ChromeEarlGrey changeWindowWithNumber:1 toNewNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kSecondURLText
                             inWindowWithNumber:0];

  // Opens a 'new' second window.
  [ChromeEarlGrey openNewWindow];
  [ChromeEarlGrey waitUntilReadyWindowWithNumber:1];
  [ChromeEarlGrey waitForForegroundWindowCount:2];

  // Loads urls in both windows, and verifies.
  [ChromeEarlGrey loadURL:firstURL inWindowWithNumber:0];
  [ChromeEarlGrey loadURL:secondURL inWindowWithNumber:1];
  [ChromeEarlGrey waitForWebStateContainingText:kFirstURLText
                             inWindowWithNumber:0];
  [ChromeEarlGrey waitForWebStateContainingText:kSecondURLText
                             inWindowWithNumber:1];
}

@end
