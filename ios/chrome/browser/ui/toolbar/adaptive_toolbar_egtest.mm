// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/ui/infobars/test_infobar_delegate.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_button.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/secondary_toolbar_view.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/bookmarks_test_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using web::test::ElementSelector;

namespace {

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

const char kPageURL[] = "/test-page.html";
const char kPageURL2[] = "/test-page-2.html";
const char kPageURL3[] = "/test-page-3.html";
const char kLinkID[] = "linkID";
const char kTextID[] = "textID";
const char kPageLoadedString[] = "Page loaded!";

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
      "<html><body><p>" + std::string(kPageLoadedString) + "</p><a href=\"" +
      kPageURL3 + "\" id=\"" + kLinkID + "\">link!</a></body></html>");
  return std::move(http_response);
}

// Provides response for a very tall page.
std::unique_ptr<net::test_server::HttpResponse> TallPageResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(
      "<html><body><p style=\"height:2000pt\"></p><p id=\"" +
      std::string(kTextID) + "\">" + kPageLoadedString + "</p></body></html>");
  return std::move(http_response);
}

// Returns a matcher for the bookmark button.
id<GREYMatcher> BookmarkButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
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
id<GREYMatcher> SearchButton() {
  return grey_accessibilityID(kToolbarOmniboxButtonIdentifier);
}

// Returns a matcher for the tab grid button.
id<GREYMatcher> TabGridButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TOOLBAR_SHOW_TABS);
}

// Returns a matcher for a UIResponder object being first responder.
id<GREYMatcher> firstResponder() {
  MatchesBlock matches = ^BOOL(UIResponder* responder) {
    return [responder isFirstResponder];
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
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
  return grey_allOf(grey_ancestor(grey_kindOfClass([PrimaryToolbarView class])),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for elements being subviews of the SecondaryToolbarView and
// sufficientlyVisible.
id<GREYMatcher> VisibleInSecondaryToolbar() {
  return grey_allOf(
      grey_ancestor(grey_kindOfClass([SecondaryToolbarView class])),
      grey_sufficientlyVisible(), nil);
}

// Checks that the element designated by |matcher| is |visible| in the primary
// toolbar.
void CheckVisibleInPrimaryToolbar(id<GREYMatcher> matcher, BOOL visible) {
  id<GREYMatcher> assertionMatcher = visible ? grey_notNil() : grey_nil();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, VisibleInPrimaryToolbar(),
                                          nil)]
      assertWithMatcher:assertionMatcher];
}

// Checks that the element designed by |matcher| is |visible| in the secondary
// toolbar.
void CheckVisibleInSecondaryToolbar(id<GREYMatcher> matcher, BOOL visible) {
  id<GREYMatcher> assertionMatcher = visible ? grey_notNil() : grey_nil();
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(matcher, VisibleInSecondaryToolbar(),
                                          nil)]
      assertWithMatcher:assertionMatcher];
}

// Returns a matcher for a UIControl object being spotlighted.
id<GREYMatcher> Spotlighted() {
  MatchesBlock matches = ^BOOL(UIControl* control) {
    return control.state & ControlStateSpotlighted;
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"is spotlighted"];
  };
  return grey_allOf(
      grey_kindOfClass([UIControl class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

bool AddInfobar() {
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(chrome_test_util::GetCurrentWebState());
  return TestInfoBarDelegate::Create(manager);
}

// Rotate the device if it is an iPhone or change the trait collection to
// compact width if it is an iPad. Returns the new trait collection.
UITraitCollection* RotateOrChangeTraitCollection(
    UITraitCollection* originalTraitCollection,
    UIViewController* topViewController) {
  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection = nil;
  if (IsIPadIdiom()) {
    // Simulate a multitasking by overriding the trait collections of the view
    // controllers. The rotation doesn't work on iPad.
    UITraitCollection* horizontalCompact = [UITraitCollection
        traitCollectionWithHorizontalSizeClass:UIUserInterfaceSizeClassCompact];
    secondTraitCollection =
        [UITraitCollection traitCollectionWithTraitsFromCollections:@[
          originalTraitCollection, horizontalCompact
        ]];
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:secondTraitCollection
                             forChildViewController:child];
    }

  } else {
    // On iPhone rotate to test the the landscape orientation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                             errorOrNil:nil];
    secondTraitCollection = topViewController.traitCollection;
  }
  return secondTraitCollection;
}

// Checks that the element associated with |matcher| is visible in the toolbar
// defined by |visibility|.
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
    if (IsRefreshLocationBarEnabled()) {
      CheckVisibleInPrimaryToolbar(chrome_test_util::DefocusedLocationView(),
                                   YES);
    } else {
      CheckVisibleInPrimaryToolbar(chrome_test_util::Omnibox(), YES);
    }
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
    CheckVisibilityInToolbar(BookmarkButton(), ButtonVisibilityNone);

    // Those buttons are hidden by the keyboard.
    CheckVisibilityInToolbar(BackButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(SearchButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityNone);
  } else {
    CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(BookmarkButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(BackButton(), ButtonVisibilitySecondary);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilitySecondary);
    CheckVisibilityInToolbar(SearchButton(), ButtonVisibilitySecondary);
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
    CheckVisibilityInToolbar(BookmarkButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(BackButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(SearchButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityNone);
  } else {
    CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(BookmarkButton(), ButtonVisibilityNone);

    CheckVisibilityInToolbar(BackButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(SearchButton(), ButtonVisibilityNone);
    CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityPrimary);
    CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityPrimary);
  }
  // The secondary toolbar is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClass([SecondaryToolbarView class])]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Check the visibility of the buttons if the device is an iPad not in
// multitasking.
void CheckButtonsVisibilityIPad() {
  CheckVisibilityInToolbar(CancelButton(), ButtonVisibilityNone);

  CheckVisibilityInToolbar(ShareButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(ReloadButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(BookmarkButton(), ButtonVisibilityPrimary);

  CheckVisibilityInToolbar(BackButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(ForwardButton(), ButtonVisibilityPrimary);
  CheckVisibilityInToolbar(SearchButton(), ButtonVisibilityNone);
  CheckVisibilityInToolbar(TabGridButton(), ButtonVisibilityNone);
  CheckVisibilityInToolbar(ToolsMenuButton(), ButtonVisibilityPrimary);

  // The secondary toolbar is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClass([SecondaryToolbarView class])]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Check that the button displayed are the ones which should be displayed in the
// environment described by |traitCollection| and with |omniboxFocused|.
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
  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        assertWithMatcher:chrome_test_util::OmniboxContainingText(string)];

    if (IsIPadIdiom()) {
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
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        assertWithMatcher:chrome_test_util::OmniboxContainingText(string)];
  }
}

void FocusOmnibox() {
  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:firstResponder()];
}

}  // namespace

#pragma mark - TestCase

// Test case for the adaptive toolbar UI.
@interface AdaptiveToolbarTestCase : ChromeTestCase

@end

@implementation AdaptiveToolbarTestCase

// Tests that bookmarks button is spotlighted for the bookmarked pages.
- (void)testBookmarkButton {
  if (!IsRegularXRegularSizeClass()) {
    EARL_GREY_TEST_SKIPPED(
        @"The bookmark button is only visible on Regular x Regular size "
        @"classes.");
  }

  // Setup the bookmarks.
  [ChromeEarlGrey waitForBookmarksToFinishLoading];
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");

  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Navigate to a page and check the bookmark button is not spotlighted.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:grey_allOf(grey_kindOfClass([UIControl class]),
                                   grey_not(Spotlighted()), nil)];

  // Bookmark the page.
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:Spotlighted()];

  // Navigate to a different page and check the button is not selected.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL2)];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:grey_allOf(grey_kindOfClass([UIControl class]),
                                   grey_not(Spotlighted()), nil)];

  // Navigate back to the bookmarked page and check the button.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];
  [[EarlGrey selectElementWithMatcher:BookmarkButton()]
      assertWithMatcher:Spotlighted()];

  // Clean the bookmarks
  GREYAssert(chrome_test_util::ClearBookmarks(),
             @"Not all bookmarks were removed.");
}

// Tests that tapping a button cancels the focus on the omnibox.
- (void)testCancelOmniboxEdit {
  if (IsCompactWidth()) {
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
  UIViewController* topViewController =
      top_view_controller::TopPresentedViewController();
  UITraitCollection* originalTraitCollection =
      topViewController.traitCollection;

  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection =
      RotateOrChangeTraitCollection(originalTraitCollection, topViewController);

  FocusOmnibox();

  // Check the visiblity when focusing the omnibox.
  CheckToolbarButtonVisibility(secondTraitCollection, YES);

  // Revert the orientation/trait collection to the original.
  if (IsIPadIdiom()) {
    // Remove the override.
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:originalTraitCollection
                             forChildViewController:child];
    }
  } else {
    // Cancel the rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                             errorOrNil:nil];
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
  UIViewController* topViewController =
      top_view_controller::TopPresentedViewController();
  UITraitCollection* originalTraitCollection =
      topViewController.traitCollection;

  // Check the button visibility.
  CheckToolbarButtonVisibility(originalTraitCollection, YES);

  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection =
      RotateOrChangeTraitCollection(originalTraitCollection, topViewController);

  // Check the visiblity after a size class change.
  CheckToolbarButtonVisibility(secondTraitCollection, YES);

  if (IsIPadIdiom()) {
    // Remove the override.
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:originalTraitCollection
                             forChildViewController:child];
    }
  } else {
    // Cancel the rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                             errorOrNil:nil];
  }

  // Check the visiblity after a size class change. This should let the trait
  // collection change come into effect.
  CheckToolbarButtonVisibility(originalTraitCollection, YES);
}

// Tests the interactions between the infobars and the bottom toolbar during
// fullscreen.
- (void)testInfobarFullscreen {
  if (!IsSplitToolbarMode()) {
    // The interaction between the infobar and fullscreen only happens in split
    // toolbar mode.
    return;
  }

  // Setup the server.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&TallPageResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  // Navigate to a page.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPageURL)];

  GREYAssert(AddInfobar(), @"Failed to add infobar.");

  [[GREYCondition
      conditionWithName:@"Waiting for infobar to show"
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:
                            chrome_test_util::StaticTextWithAccessibilityLabel(
                                base::SysUTF8ToNSString(kTestInfoBarTitle))]
                        assertWithMatcher:grey_sufficientlyVisible()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:4];

  // Check that the button is visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  UIWindow* window = [[UIApplication sharedApplication] keyWindow];

  GREYElementMatcherBlock* positionMatcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(UIView* element) {
        UILayoutGuide* guide =
            [NamedGuide guideWithName:kSecondaryToolbarGuide view:element];
        CGFloat toolbarTopPoint = CGRectGetMinY(
            [window convertRect:guide.layoutFrame fromView:guide.owningView]);
        CGFloat buttonBottomPoint = CGRectGetMaxY(
            [window convertRect:element.frame fromView:element.superview]);

        CGFloat bottomSafeArea = CGFLOAT_MAX;
        if (@available(iOS 11, *)) {
          bottomSafeArea =
              CGRectGetMaxY(window.safeAreaLayoutGuide.layoutFrame);
        }
        CGFloat infobarContentBottomPoint =
            MIN(bottomSafeArea, toolbarTopPoint);
        BOOL buttonIsAbove = buttonBottomPoint < infobarContentBottomPoint - 10;
        BOOL buttonIsNear = buttonBottomPoint > infobarContentBottomPoint - 30;
        return buttonIsAbove && buttonIsNear;
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:@"Infobar is position on top of the bottom toolbar."];
      }];

  // Check that the button is positionned above the bottom toolbar.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      assertWithMatcher:positionMatcher];

  // Scroll down
  [[EarlGrey
      selectElementWithMatcher:web::WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];

  // Check that the button is visible.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Check that the secondary toolbar is not visible.
  [[EarlGrey
      selectElementWithMatcher:grey_kindOfClass([SecondaryToolbarView class])]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];

  // Check that the button is positionned above the bottom toolbar (i.e. at the
  // bottom).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OKButton()]
      assertWithMatcher:positionMatcher];
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
  [[EarlGrey
      selectElementWithMatcher:web::WebViewInWebState(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        ElementSelector::ElementSelectorId(kLinkID),
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::StaticTextWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)]
      performAction:grey_tap()];

  // Check the buttons status.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Tests that tapping the omnibox button focuses the omnibox.
- (void)testOmniboxButton {
  if (!IsSplitToolbarMode()) {
    EARL_GREY_TEST_SKIPPED(@"No omnibox button to tap.");
  }

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kToolbarOmniboxButtonIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:firstResponder()];
}

// Tests share button is enabled only on pages that can be shared.
- (void)testShareButton {
  if (!IsIPadIdiom()) {
    // If this test is run on an iPhone, rotate it to have the unsplit toolbar.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeLeft
                             errorOrNil:nil];
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

  if (!IsIPadIdiom()) {
    // Cancel rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                             errorOrNil:nil];
  }
}

// Test that the bottom toolbar is still visible after closing the last
// incognito tab using long press. See https://crbug.com/849937.
- (void)testBottomToolbarHeightAfterClosingTab {
  if (!IsSplitToolbarMode())
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
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdleWithTimeout:2];

  [[self class] closeAllTabs];
  chrome_test_util::OpenNewTab();

  // Check that the bottom toolbar is visible.
  [[EarlGrey selectElementWithMatcher:SearchButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Verifies the existence and state of toolbar UI elements.
- (void)testToolbarUI {
  // Load a page to have the toolbar visible (hidden on NTP).
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Get the original trait collection.
  UIViewController* topViewController =
      top_view_controller::TopPresentedViewController();
  UITraitCollection* originalTraitCollection =
      topViewController.traitCollection;

  // Check the button visibility.
  CheckToolbarButtonVisibility(originalTraitCollection, NO);

  // Change the orientation or the trait collection.
  UITraitCollection* secondTraitCollection =
      RotateOrChangeTraitCollection(originalTraitCollection, topViewController);

  // Check the visiblity after a size class change.
  CheckToolbarButtonVisibility(secondTraitCollection, NO);

  if (IsIPadIdiom()) {
    // Remove the override.
    for (UIViewController* child in topViewController.childViewControllers) {
      [topViewController setOverrideTraitCollection:originalTraitCollection
                             forChildViewController:child];
    }
  } else {
    // Cancel the rotation.
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                             errorOrNil:nil];
  }
}

@end
