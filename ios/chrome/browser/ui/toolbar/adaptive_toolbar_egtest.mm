// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/toolbar/adaptive_toolbar_app_interface.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;
using chrome_test_util::WebStateScrollViewMatcher;
using chrome_test_util::WebViewMatcher;

const char kPageURL[] = "/test-page.html";
const char kPageURL2[] = "/test-page-2.html";
const char kPageURL3[] = "/test-page-3.html";
const char kLinkID[] = "linkID";
const char kPageLoadedString[] = "Page loaded!";

// The title of the test infobar.
NSString* kTestInfoBarTitle = @"TestInfoBar";

// Defines the visibility of an element, in relation to the toolbar.
typedef NS_ENUM(NSInteger, ButtonVisibility) {
  ButtonVisibilityNone,
  ButtonVisibilityPrimary,
  ButtonVisibilitySecondary
};

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<html><body><p>" + std::string(kPageLoadedString) +
      "</p><a style='font-size:50pt' href=\"" + kPageURL3 + "\" id=\"" +
      kLinkID + "\">link!</a></body></html>");
  return std::move(http_response);
}

// Returns a matcher for the visible share button.
id<GREYMatcher> ShareButton() {
  return grey_allOf(grey_accessibilityID(kToolbarShareButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the reload button.
id<GREYMatcher> ReloadButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCNAME_RELOAD);
}

// Returns a matcher for the tools menu button.
id<GREYMatcher> ToolsMenuButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TOOLBAR_SETTINGS);
}

// Returns a matcher for the cancel button.
id<GREYMatcher> CancelButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(IDS_CANCEL);
}

// Returns a matcher for the search button.
id<GREYMatcher> NewTabButton() {
  return grey_accessibilityID(kToolbarNewTabButtonIdentifier);
}

// Returns a matcher for the tab grid button.
id<GREYMatcher> TabGridButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TOOLBAR_SHOW_TABS);
}

// Returns a matcher for a UIResponder object being first responder.
id<GREYMatcher> firstResponder() {
  GREYMatchesBlock matches = ^BOOL(UIResponder* responder) {
    return [responder isFirstResponder];
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"first responder"];
  };
  return grey_allOf(
      grey_kindOfClass([UIResponder class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

// Returns a matcher for elements being subviews of the PrimaryToolbarView and
// sufficientlyVisible.
id<GREYMatcher> VisibleInPrimaryToolbar() {
  return grey_allOf(grey_ancestor(grey_kindOfClassName(@"PrimaryToolbarView")),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for elements being subviews of the SecondaryToolbarView and
// sufficientlyVisible.
id<GREYMatcher> VisibleInSecondaryToolbar() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClassName(@"SecondaryToolbarView")),
      grey_sufficientlyVisible(), nil);
}

// Checks that the element designated by `matcher` is `visible` in the primary
// toolbar.
void CheckVisibleInPrimaryToolbar(id<GREYMatcher> matcher, BOOL visible) {
  id<GREYMatcher> assertionMatcher = visible ? grey_notNil() : grey_nil();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, VisibleInPrimaryToolbar(),
                                          nil)]
      assertWithMatcher:assertionMatcher];
}

// Checks that the element designed by `matcher` is `visible` in the secondary
// toolbar.
void CheckVisibleInSecondaryToolbar(id<GREYMatcher> matcher, BOOL visible) {
  id<GREYMatcher> assertionMatcher = visible ? grey_notNil() : grey_nil();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, VisibleInSecondaryToolbar(),
                                          nil)]
      assertWithMatcher:assertionMatcher];
}

// Rotate the device if it is an iPhone or change the trait collection to
// compact width if it is an iPad. Returns the new trait collection.
UITraitCollection* RotateOrChangeTraitCollection(
    UITraitCollection* originalTraitCollection,
    UIViewController* topViewController) {
  // Change the orientation or the trait collection.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Simulate a multitasking by overriding the trait collections of the view
    // controllers. The rotation doesn't work on iPad.
    return [AdaptiveToolbarAppInterface
        changeTraitCollection:originalTraitCollection
            forViewController:topViewController];
  } else {
    // On iPhone rotate to test the the landscape orientation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                  error:nil];
    return topViewController.traitCollection;
  }
}

// Checks that the element associated with `matcher` is visible in the toolbar
// defined by `visibility`.
void CheckVisibilityInToolbar(id<GREYMatcher> matcher,
                              ButtonVisibility visibility) {
  CheckVisibleInPrimaryToolbar(matcher, visibility == ButtonVisibilityPrimary);
  CheckVisibleInSecondaryToolbar(matcher,
                                 visibility == ButtonVisibilitySecondary);
}

// Checks the visibility of the different part of the omnibox, depending on it
// being focused or not.
void CheckOmniboxVisibility(BOOL omniboxFocused) {
  // Check omnibox/steady view visibility.
  if (omniboxFocused) {
    // Check that the omnibox is visible.
    CheckVisibleInPrimaryToolbar(chrome_test_util::Omnibox(), YES);
  } else {
    // Check that location view is visible.
      CheckVisibleInPrimaryToolbar(chrome_test_util::DefocusedLocationView(),
                                   YES);
  }
}

// Check the visibility of the buttons if the device is an iPhone in portrait or
// an iPad in multitasking.
void CheckButtonsVisibilityIPhonePortrait(BOOL omniboxFocused) {
  // Check that the cancel button visibility.
  if (omniboxFocused) {
    CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityPrimary);

    CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityNone);

    // Those buttons are hidden by the keyboard.
    CheckVisibilityInToolbar(BackButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(NewTabButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityNone);
  } else {
    CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(BackButton(), ButtonVisibilitySecondary);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilitySecondary);
    CheckVisibilityInToolbar(NewTabButton(), ButtonVisibilitySecondary);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilitySecondary);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilitySecondary);
  }
}

// Check the visibility of the buttons if the device is an iPhone in landscape.
void CheckButtonsVisibilityIPhoneLandscape(BOOL omniboxFocused) {
  if (omniboxFocused) {
    // Omnibox focused in iPhone landscape.
    CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityPrimary);

    CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(BackButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(NewTabButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityNone);
  } else {
    CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityPrimary);

    CheckVisibilityInToolbar(BackButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(NewTabButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityPrimary);
  }
  // The secondary toolbar is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"SecondaryToolbarView")]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Check the visibility of the buttons if the device is an iPad not in
// multitasking.
void CheckButtonsVisibilityIPad() {
  CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityNone);

  CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityPrimary);

  CheckVisibilityInToolbar(BackButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(NewTabButton(), ButtonVisibilityNone);
  CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityPrimary);

  // The secondary toolbar is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClassName(@"SecondaryToolbarView")]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Check that the button displayed are the ones which should be displayed in the
// environment described by `traitCollection` and with `omniboxFocused`.
void CheckToolbarButtonVisibility(UITraitCollection* traitCollection,
                                  BOOL omniboxFocused) {
  CheckOmniboxVisibility(omniboxFocused);

  // Button checks.
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    CheckButtonsVisibilityIPhonePortrait(omniboxFocused);
  } else if (traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassCompact) {
    CheckButtonsVisibilityIPhoneLandscape(omniboxFocused);
  } else {
    CheckButtonsVisibilityIPad();
  }
}

// Check that current URL contains a given string by tapping on the location
// view to focus the omnibox where the full URL can be seen, then comparing
// the strings, and finally defocusing the omnibox.
void CheckCurrentURLContainsString(std::string string) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        assertWithMatcher:chrome_test_util::OmniboxContainingText(string)];

    if ([ChromeEarlGrey isIPadIdiom]) {
      // Defocus omnibox by tapping the typing shield.
      [[EarlGrey
          selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
          performAction:grey_tap()];

    } else {
      [[EarlGrey
          selectElementWithMatcher:
              grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier)]
          performAction:grey_tap()];
    }
}

void FocusOmnibox() {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:firstResponder()];
}

UIViewController* TopPresentedViewControllerFrom(
    UIViewController* base_view_controller) {
  UIViewController* topController = base_view_controller;
  for (UIViewController* controller = [topController presentedViewController];
       controller && ![controller isBeingDismissed];
       controller = [controller presentedViewController]) {
    topController = controller;
  }
  return topController;
}

UIViewController* TopPresentedViewController() {
  UIViewController* rootViewController =
      chrome_test_util::GetAnyKeyWindow().rootViewController;
  return TopPresentedViewControllerFrom(rootViewController);
}

}  // namespace

#pragma mark - TestCase

// Test case for the adaptive toolbar UI.
@interface AdaptiveToolbarTestCase : ChromeTestCase

@end

@implementation AdaptiveToolbarTestCase

// Tests that tapping a button cancels the focus on the omnibox.
- (void)testCancelOmniboxEdit {
  if ([ChromeEarlGrey isCompactWidth]) {
    EARL_GREY_TEST_SKIPPED(@"No button to tap in compact width.");
  }

  // Navigate to a page to enable the back button.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  FocusOmnibox();

  // Tap the back button and check the omnibox is unfocused.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_not(firstResponder())];
}

// Check the button visibility of the toolbar when the omnibox is focused from a
// different orientation than the default one.
- (void)testFocusOmniboxFromOtherOrientation {
  // Load a page to have the toolbar visible (hidden on NTP).
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Get the original trait collection.
  UIViewController* topViewController = TopPresentedViewController();
  UITraitCollection* originalTraitCollection =
      topViewController.traitCollection;

  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection =
      RotateOrChangeTraitCollection(originalTraitCollection, topViewController);

  FocusOmnibox();

  // Check the visiblity when focusing the omnibox.
  CheckToolbarButtonVisibility(secondTraitCollection, YES);

  // Revert the orientation/trait collection to the original.
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Remove the override.
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:originalTraitCollection
                             forChildViewController:child];
    }
  } else {
    // Cancel the rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  }

  // Check the visiblity after a rotation.
  CheckToolbarButtonVisibility(originalTraitCollection, YES);
}

// Check the button visibility of the toolbar when the omnibox is focused from
// the default orientation.
- (void)testFocusOmniboxFromPortrait {
  // Load a page to have the toolbar visible (hidden on NTP).
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  FocusOmnibox();

  // Get the original trait collection.
  UIViewController* topViewController = TopPresentedViewController();
  UITraitCollection* originalTraitCollection =
      topViewController.traitCollection;

  // Check the button visibility.
  CheckToolbarButtonVisibility(originalTraitCollection, YES);

  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection =
      RotateOrChangeTraitCollection(originalTraitCollection, topViewController);

  // Check the visiblity after a size class change.
  CheckToolbarButtonVisibility(secondTraitCollection, YES);

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Remove the override.
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:originalTraitCollection
                             forChildViewController:child];
    }
  } else {
    // Cancel the rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  }

  // Check the visiblity after a size class change. This should let the trait
  // collection change come into effect.
  CheckToolbarButtonVisibility(originalTraitCollection, YES);
}

// Verifies that the back/forward buttons are working and are correctly enabled
// during navigations.
- (void)testNavigationButtons {
  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Loads two url and check the navigation buttons status.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL2)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Check the navigation to the second page occurred.
  CheckCurrentURLContainsString(kPageURL2);

  // Go back.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  CheckCurrentURLContainsString(kPageURL);

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_interactable()];

  // Go forward.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      performAction:grey_tap()];
  CheckCurrentURLContainsString(kPageURL2);

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];

  // Open a page in a new incognito tab to have the focus.
  [[EarlGrey selectElementWithMatcher:WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE)]
      performAction:grey_tap()];

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that tapping the NewTab button opens a new tab.
- (void)testNewTabButton {
  if (![ChromeEarlGrey isSplitToolbarMode]) {
    EARL_GREY_TEST_SKIPPED(@"No button to tap.");
  }

  [ChromeEarlGrey waitForMainTabCount:1];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolbarNewTabButtonIdentifier)]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests share button is enabled only on pages that can be shared.
- (void)testShareButton {
  if (![ChromeEarlGrey isIPadIdiom]) {
    // If this test is run on an iPhone, rotate it to have the unsplit toolbar.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                                  error:nil];
  }

  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Navigate to another page and check that the share button is enabled.
  [ChromeEarlGrey loadURL:pageURL];
  [[EarlGrey selectElementWithMatcher:ShareButton()]
      assertWithMatcher:grey_interactable()];

  if (![ChromeEarlGrey isIPadIdiom]) {
    // Cancel rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  }
}

// Test that the bottom toolbar is still visible after closing the last
// incognito tab using long press. See https://crbug.com/849937.
- (void)testBottomToolbarHeightAfterClosingTab {
  if (![ChromeEarlGrey isSplitToolbarMode])
    EARL_GREY_TEST_SKIPPED(@"This test needs a bottom toolbar.");
  // Close all tabs.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridCloseButtonForCellAtIndex(0)]
      performAction:grey_tap()];

  // Open incognito tab.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridIncognitoTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridNewIncognitoTabButton()]
      performAction:grey_tap()];
  GREYWaitForAppToIdleWithTimeout(5.0, @"App failed to idle");

  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Check that the bottom toolbar is visible.
  [[EarlGrey selectElementWithMatcher:NewTabButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies the existence and state of toolbar UI elements.
- (void)testToolbarUI {
  // Load a page to have the toolbar visible (hidden on NTP).
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Get the original trait collection.
  UIViewController* topViewController = TopPresentedViewController();
  UITraitCollection* originalTraitCollection =
      topViewController.traitCollection;

  // Check the button visibility.
  CheckToolbarButtonVisibility(originalTraitCollection, NO);

  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection =
      RotateOrChangeTraitCollection(originalTraitCollection, topViewController);

  // Check the visiblity after a size class change.
  CheckToolbarButtonVisibility(secondTraitCollection, NO);

  if ([ChromeEarlGrey isIPadIdiom]) {
    // Remove the override.
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:originalTraitCollection
                             forChildViewController:child];
    }
  } else {
    // Cancel the rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
  }
}

@end
