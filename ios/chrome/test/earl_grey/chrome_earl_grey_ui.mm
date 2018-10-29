// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"

#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/privacy_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ClearBrowsingDataCollectionView;
using chrome_test_util::SettingsMenuButton;
using chrome_test_util::ToolsMenuView;
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
}  // namespace

@implementation ChromeEarlGreyUI

+ (void)openToolsMenu {
  // TODO(crbug.com/639524): Add logic to ensure the app is in the correct
  // state, for example DCHECK if no tabs are displayed.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(chrome_test_util::ToolsMenuButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionDown)
      onElementWithMatcher:web::WebViewScrollView(
                               chrome_test_util::GetCurrentWebState())]
      performAction:grey_tap()];
  // TODO(crbug.com/639517): Add webViewScrollView matcher so we don't have
  // to always find it.
}

+ (void)openSettingsMenu {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:SettingsMenuButton()];
}

+ (void)tapToolsMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableSettingsButton =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableSettingsButton]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:ToolsMenuView()] performAction:grey_tap()];
}

+ (void)tapSettingsMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];
}

+ (void)tapClearBrowsingDataMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:ClearBrowsingDataCollectionView()]
      performAction:grey_tap()];
}

+ (void)openAndClearBrowsingDataFromHistory {
  // Open Clear Browsing Data Button
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kHistoryToolbarClearBrowsingButtonIdentifier)]
      performAction:grey_tap()];

  // Uncheck "Cookies, Site Data" and "Cached Images and Files," which are
  // checked by default, and press "Clear Browsing Data"
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kClearBrowsingDataButtonIdentifier)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Wait until activity indicator modal is cleared, meaning clearing browsing
  // data has been finished.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Include sufficientlyVisible condition for the case of the clear browsing
  // dialog, which also has a "Done" button and is displayed over the history
  // panel.
  id<GREYMatcher> visibleDoneButton = grey_allOf(
      chrome_test_util::SettingsDoneButton(), grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:visibleDoneButton]
      performAction:grey_tap()];
}

+ (void)assertHistoryHasNoEntries {
  id<GREYMatcher> noHistoryMessageMatcher =
      grey_allOf(grey_text(l10n_util::GetNSString(IDS_HISTORY_NO_RESULTS)),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noHistoryMessageMatcher]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> historyEntryMatcher =
      grey_allOf(grey_kindOfClass([TableViewURLCell class]),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:historyEntryMatcher]
      assertWithMatcher:grey_nil()];
}

+ (void)tapPrivacyMenuButton:(id<GREYMatcher>)buttonMatcher {
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:grey_accessibilityID(kPrivacyCollectionViewId)]
      performAction:grey_tap()];
}

+ (void)tapAccountsMenuButton:(id<GREYMatcher>)buttonMatcher {
  [[[EarlGrey selectElementWithMatcher:buttonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:grey_accessibilityID(kSettingsAccountsId)]
      performAction:grey_tap()];
}

+ (void)focusOmniboxAndType:(NSString*)text {
  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(text)];
}

+ (void)openNewTab {
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewTabId);
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

+ (void)openNewIncognitoTab {
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newIncognitoTabMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabMatcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

+ (void)reload {
  // On iPhone Reload button is a part of tools menu, so open it.
  if (IsCompactWidth()) {
    [self openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ReloadButton()]
      performAction:grey_tap()];
}

+ (void)openShareMenu {
  if (IsCompactWidth() && !IsRefreshLocationBarEnabled()) {
    [ChromeEarlGreyUI openToolsMenu];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShareButton()]
      performAction:grey_tap()];
}

+ (void)waitForToolbarVisible:(BOOL)isVisible {
  const NSTimeInterval kWaitForToolbarAnimationTimeout = 1.0;
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
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitForToolbarAnimationTimeout, condition),
             errorMessage);
}

@end
