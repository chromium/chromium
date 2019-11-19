// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"

#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Redefine EarlGrey macro to use line number and file name taken from the place
// of ChromeEarlGreyUI macro instantiation, rather than local line number
// inside test helper method. Original EarlGrey macro definition also expands to
// EarlGreyImpl instantiation. [self earlGrey] is provided by a superclass and
// returns EarlGreyImpl object created with correct line number and filename.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define EarlGrey [self earlGrey]
#pragma clang diagnostic pop

using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ConfirmClearBrowsingDataButton;
using chrome_test_util::SettingsMenuButton;
using chrome_test_util::ToolsMenuView;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Returns a GREYAction to scroll down (swipe up) for a reasonably small amount.
id<GREYAction> ScrollDown() {
  // 150 is a reasonable value to ensure all menu items are seen, without too
  // much delay. With a larger value, some menu items could be skipped while
  // searching. A smaller value increses the area that is searched, but slows
  // down the scroll.
  CGFloat const kMenuScrollDisplacement = 150;
  return grey_scrollInDirection(kGREYDirectionDown, kMenuScrollDisplacement);
}

bool IsAppCompactWidth() {
#if defined(CHROME_EARL_GREY_1)
  UIApplication* application = [UIApplication sharedApplication];
  UIWindow* keyWindow = application.keyWindow;
  UIUserInterfaceSizeClass sizeClass =
      keyWindow.traitCollection.horizontalSizeClass;
#elif defined(CHROME_EARL_GREY_2)
  UIApplication* remoteApplication =
      [GREY_REMOTE_CLASS_IN_APP(UIApplication) sharedApplication];
  UIWindow* remoteKeyWindow = remoteApplication.keyWindow;
  UIUserInterfaceSizeClass sizeClass =
      remoteKeyWindow.traitCollection.horizontalSizeClass;
#endif

  return sizeClass == UIUserInterfaceSizeClassCompact;
}

}  // namespace

@implementation ChromeEarlGreyUIImpl

- (void)openToolsMenu {
  // TODO(crbug.com/639524): Add logic to ensure the app is in the correct
  // state, for example DCHECK if no tabs are displayed.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(chrome_test_util::ToolsMenuButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionDown)
      onElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
      performAction:grey_tap()];
  // TODO(crbug.com/639517): Add webViewScrollView matcher so we don't have
  // to always find it.
}

- (void)openSettingsMenu {
  [self openToolsMenu];
  [self tapToolsMenuButton:SettingsMenuButton()];
}

- (void)tapToolsMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableSettingsButton =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableSettingsButton]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:ToolsMenuView()] performAction:grey_tap()];
}

- (void)tapSettingsMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];
}

- (void)tapClearBrowsingDataMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:ClearBrowsingDataView()] performAction:grey_tap()];
}

- (void)openAndClearBrowsingDataFromHistory {
  // Open Clear Browsing Data Button
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Uncheck "Cookies, Site Data" and "Cached Images and Files," which are
  // checked by default, and press "Clear Browsing Data"
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ClearCacheButton()]
      performAction:grey_tap()];

  // Set 'Time Range' to 'All Time'.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::ButtonWithAccessibilityLabelId(
              IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME)]
      performAction:grey_tap()];
  [[[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsMenuBackButton()]
      atIndex:0] performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ClearBrowsingDataButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Wait until activity indicator modal is cleared, meaning clearing browsing
  // data has been finished.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Recheck "Cookies, Site Data" and "Cached Images and Files."
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ClearCacheButton()]
      performAction:grey_tap()];

  // Include sufficientlyVisible condition for the case of the clear browsing
  // dialog, which also has a "Done" button and is displayed over the history
  // panel.
  id<GREYMatcher> visibleDoneButton = grey_allOf(
      chrome_test_util::SettingsDoneButton(), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:visibleDoneButton]
      performAction:grey_tap()];
}

- (void)assertHistoryHasNoEntries {
  id<GREYMatcher> noHistoryMessageMatcher =
      grey_allOf(grey_text(l10n_util::GetNSString(IDS_HISTORY_NO_RESULTS)),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noHistoryMessageMatcher]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> historyEntryMatcher =
      grey_allOf(grey_kindOfClassName(@"TableViewURLCell"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:historyEntryMatcher]
      assertWithMatcher:grey_nil()];
}

- (void)tapPrivacyMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsPrivacyTableView()]
      performAction:grey_tap()];
}

- (void)tapAccountsMenuButton:(id<GREYMatcher>)buttonMatcher {
  [[[EarlGrey selectElementWithMatcher:buttonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
}

- (void)focusOmniboxAndType:(NSString*)text {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];

  if (text.length) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_typeText(text)];
  }
}

- (void)focusOmnibox {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_tap()];
}

- (void)openNewTab {
  [self openToolsMenu];
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewTabId);
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)openNewIncognitoTab {
  [self openToolsMenu];
  id<GREYMatcher> newIncognitoTabMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabMatcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

- (void)reload {
  // On iPhone Reload button is a part of tools menu, so open it.
  if (IsAppCompactWidth()) {
    [self openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
      performAction:grey_tap()];
}

- (void)openShareMenu {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShareButton()]
      performAction:grey_tap()];
}

- (void)waitForToolbarVisible:(BOOL)isVisible {
  ConditionBlock condition = ^{
    NSError* error = nil;
    id<GREYMatcher> visibleMatcher = isVisible ? grey_notNil() : grey_nil();
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        assertWithMatcher:visibleMatcher
                    error:&error];
    return error == nil;
  };
  NSString* errorMessage =
      isVisible ? @"Toolbar was not visible" : @"Toolbar was visible";

  bool toolbarVisibility = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, condition);
  EG_TEST_HELPER_ASSERT_TRUE(toolbarVisibility, errorMessage);
}

@end
