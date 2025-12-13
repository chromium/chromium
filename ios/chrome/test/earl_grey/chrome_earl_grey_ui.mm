// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_container/ui_bundled/edit_menu_app_interface.h"
#import "ios/chrome/browser/popup_menu/ui_bundled/popup_menu_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/chrome/test/earl_grey/scoped_disable_timer_tracking.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/element_selector.h"
#import "ui/base/l10n/l10n_util.h"

// Redefine EarlGrey macro to use line number and file name taken from the place
// of ChromeEarlGreyUI macro instantiation, rather than local line number
// inside test helper method. Original EarlGrey macro definition also expands to
// EarlGreyImpl instantiation. [self earlGrey] is provided by a superclass and
// returns EarlGreyImpl object created with correct line number and filename.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#define EarlGrey [self earlGrey]
#pragma clang diagnostic pop

using base::test::ios::kWaitForClearBrowsingDataTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;
using chrome_test_util::BrowsingDataButtonMatcher;
using chrome_test_util::BrowsingDataConfirmButtonMatcher;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearAutofillButton;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ClearSavedPasswordsButton;
using chrome_test_util::ContextMenuItemWithAccessibilityLabel;
using chrome_test_util::SettingsActionButton;
using chrome_test_util::SettingsDestinationButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::ToolsMenuView;

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

// Returns a GREYAction to scroll down (swipe up) for a reasonably small amount.
id<GREYAction> PageSheetScrollDown() {
  // 500 is a reasonable value to ensure all menu items are seen, and cause the
  // page sheet to expand to full screen. With a larger value, some menu items
  // could be skipped while searching. A smaller value increses the area that is
  // searched, but slows down the scroll. It also causes the page sheet to not
  // expand.
  CGFloat menu_scroll_displacement = 500;

  // On iPad we do not need the page sheet to expand to full screen and the
  // current large size may miss items in the middle of the page menu.
  if ([ChromeEarlGrey isIPadIdiom]) {
    menu_scroll_displacement = 250;
  }

  // But for very small devices (like the SE), this is too big.
  UIWindow* currentWindow = [ChromeEarlGreyAppInterface keyWindow];
  if (currentWindow.rootViewController.view.frame.size.height < 600) {
    menu_scroll_displacement = 250;
  }

  // And for iOS 26, the updated table view layout also makes this too big.
  if (@available(iOS 19.0, *)) {
    menu_scroll_displacement = 250;
  }
  return grey_scrollInDirection(kGREYDirectionDown, menu_scroll_displacement);
}

// Returns a GREYAction to scroll right (swipe left) for a reasonably small
// amount.
id<GREYAction> ScrollRight() {
  // 150 is a reasonable value to ensure all menu items are seen, without too
  // much delay. With a larger value, some menu items could be skipped while
  // searching. A smaller value increses the area that is searched, but slows
  // down the scroll.
  CGFloat const kMenuScrollDisplacement = 150;
  return grey_scrollInDirection(kGREYDirectionRight, kMenuScrollDisplacement);
}

bool IsAppCompactWidth() {
  UIUserInterfaceSizeClass sizeClass =
      chrome_test_util::GetAnyKeyWindow().traitCollection.horizontalSizeClass;

  return sizeClass == UIUserInterfaceSizeClassCompact;
}

// Maximum number of times `typeTextInOmnibox:andPressEnter:` will attempt to
// type the given text in the Omnibox. If it still cannot be typed properly
// after this number of attempts, `GREYAssert` is invoked.
const int kMaxNumberOfAttemptsAtTypingTextInOmnibox = 3;

}  // namespace

@implementation ChromeEarlGreyUIImpl

- (void)openToolsMenu {
  // TODO(crbug.com/41271107): Add logic to ensure the app is in the correct
  // state, for example DCHECK if no tabs are displayed.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:
                      grey_allOf(chrome_test_util::ToolsMenuButton(),
                                 grey_sufficientlyVisible(), nil)];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(chrome_test_util::ToolsMenuButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionDown)
      onElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
      performAction:grey_tap()];
  // TODO(crbug.com/41271101): Add webViewScrollView matcher so we don't have
  // to always find it.
}

- (void)closeToolsMenu {
  if ([ChromeEarlGrey isNewOverflowMenuEnabled] &&
      [ChromeEarlGrey isCompactWidth]) {
    // With the new overflow menu on compact devices, the half sheet covers the
    // bottom half of the screen. Swiping down on the sheet will close the menu.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
        performAction:grey_swipeFastInDirection(kGREYDirectionDown)];

    // Sometimes the menu can be expanded to full height, so one swipe isn't
    // enough to dismiss. If the menu is still visible, swipe one more time to
    // guarantee closing.
    NSError* error;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    if (error) {
      [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuView()]
          performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
    }
    return;
  }
  if (@available(iOS 26.0, *)) {
    // In iOS26, the assumption that a scrim coverts the whole window is
    // violated. Therefore, the solution is to tap on the PopoverDismissRegion
    // embedded within the ToolsMenu to dismiss the popover.
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(@"PopoverDismissRegion")]
        performAction:grey_tap()];
  } else {
    // A scrim covers the whole window and tapping on this scrim dismisses the
    // tools menu.  The "Tools Menu" button happens to be outside of the bounds
    // of the menu and is a convenient place to tap to activate the scrim.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        performAction:grey_tap()];
  }
}

- (void)openToolsMenuInWindowWithNumber:(int)windowNumber {
  [EarlGrey setRootMatcherForSubsequentInteractions:
                chrome_test_util::WindowWithNumber(windowNumber)];
  // TODO(crbug.com/41271107): Add logic to ensure the app is in the correct
  // state, for example DCHECK if no tabs are displayed.
  chrome_test_util::TapAtOffsetOf(kToolbarToolsMenuButtonIdentifier,
                                  windowNumber, CGVectorMake(0.5, 0.5));
}

- (void)openSettingsMenu {
  [self openToolsMenu];
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    [self tapToolsMenuButton:SettingsDestinationButton()];
  } else {
    [self tapToolsMenuButton:SettingsActionButton()];
  }
}

- (void)openSettingsMenuInWindowWithNumber:(int)windowNumber {
  [self openToolsMenuInWindowWithNumber:windowNumber];
  if ([ChromeEarlGrey isNewOverflowMenuEnabled]) {
    [self tapToolsMenuButton:SettingsDestinationButton()];
  } else {
    [self tapToolsMenuButton:SettingsActionButton()];
  }
}

- (void)openNewTabMenu {
  // TODO(crbug.com/41271107): Add logic to ensure the app is in the correct
  // state, for example DCHECK if no tabs are displayed.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(chrome_test_util::NewTabButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionDown)
      onElementWithMatcher:chrome_test_util::WebStateScrollViewMatcher()]
      performAction:grey_longPress()];
  // TODO(crbug.com/41271101): Add webViewScrollView matcher so we don't have
  // to always find it.
}

- (void)tapToolsMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableSettingsButton =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  id<GREYAction> scrollAction =
      [ChromeEarlGrey isNewOverflowMenuEnabled] ? ScrollRight() : ScrollDown();
  [[[EarlGrey selectElementWithMatcher:interactableSettingsButton]
         usingSearchAction:scrollAction
      onElementWithMatcher:ToolsMenuView()] performAction:grey_tap()];
}

- (void)tapToolsMenuAction:(id<GREYMatcher>)buttonMatcher {
  if (![ChromeEarlGrey isNewOverflowMenuEnabled]) {
    [self tapToolsMenuButton:buttonMatcher];
    return;
  }
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableSettingsButton =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableSettingsButton]
         usingSearchAction:PageSheetScrollDown()
      onElementWithMatcher:grey_accessibilityID(
                               kPopupMenuToolsMenuActionListId)]
      performAction:grey_tap()];
}

- (void)tapSettingsMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsCollectionView()]
      performAction:grey_tap()];
}

- (void)tapClearBrowsingDataMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
      performAction:grey_tap()];
}

- (void)openAndClearBrowsingDataFromHistory {
  // Open Clear Browsing Data Button
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          HistoryClearBrowsingDataButton()]
      performAction:grey_tap()];
  [self waitForClearBrowsingDataViewVisible:YES];
  [self selectAllBrowsingDataAndClear];
}

- (void)assertHistoryHasNoEntries {
  // Make sure the empty state illustration, title and subtitle are present.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kTableViewIllustratedEmptyViewID)]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> noHistoryTitleMatcher =
      grey_allOf(grey_text(l10n_util::GetNSString(IDS_IOS_HISTORY_EMPTY_TITLE)),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noHistoryTitleMatcher]
      assertWithMatcher:grey_notNil()];

  id<GREYMatcher> noHistoryMessageMatcher = grey_allOf(
      grey_text(l10n_util::GetNSString(IDS_IOS_HISTORY_EMPTY_MESSAGE)),
      grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:noHistoryMessageMatcher]
      assertWithMatcher:grey_notNil()];

  // Make sure there are no history entry cells.
  id<GREYMatcher> historyEntryMatcher =
      grey_allOf(grey_kindOfClassName(@"UITableViewCell"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:historyEntryMatcher]
      assertWithMatcher:grey_nil()];
}

- (void)tapPrivacyMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsPrivacyTableView()]
      performAction:grey_tap()];
}

- (void)tapPrivacySafeBrowsingMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::
                               SettingsPrivacySafeBrowsingTableView()]
      performAction:grey_tap()];
}

- (void)tapPriceNotificationsMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsNotificationsTableView()]
      performAction:grey_tap()];
}

- (void)tapTrackingPriceMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  id<GREYMatcher> interactableButtonMatcher =
      grey_allOf(buttonMatcher, grey_interactable(), nil);
  [[[EarlGrey selectElementWithMatcher:interactableButtonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsTrackingPriceTableView()]
      performAction:grey_tap()];
}

- (void)tapAccountsMenuButton:(id<GREYMatcher>)buttonMatcher {
  ScopedDisableTimerTracking disabler;
  [[[EarlGrey selectElementWithMatcher:buttonMatcher]
         usingSearchAction:ScrollDown()
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
}

- (void)focusOmniboxAndType:(NSString*)text {
  [self focusOmnibox];

  if (text.length) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_typeText(text)];
  }
}

- (void)pressEnter {
  // Press enter to navigate.
  // TODO(crbug.com/40916974): Use simulatePhysicalKeyboardEvent until
  // replaceText can properly handle \n.
  [ChromeEarlGrey simulatePhysicalKeyboardEvent:@"\n" flags:0];
}

- (void)replaceTextInOmnibox:(NSString*)text {
  if (text.length) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:chrome_test_util::NotifyChangeTextInRange(text)];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_replaceText(text)];
  }
}

- (void)focusOmniboxAndReplaceText:(NSString*)text {
  [self focusOmnibox];
  [self replaceTextInOmnibox:text];
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
  [self waitForAppToIdle];
}

- (void)openNewIncognitoTab {
  [self openToolsMenu];
  id<GREYMatcher> newIncognitoTabMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabMatcher]
      performAction:grey_tap()];
  [self waitForAppToIdle];
}

- (void)openTabGrid {
  // Wait until the button is visible because other UI may still be animating
  // and EarlGrey synchronization is disabled below which would prevent waiting.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  bool tabGridButtonVisible = base::test::ios::WaitUntilConditionOrTimeout(
      kWaitForUIElementTimeout, condition);
  EG_TEST_HELPER_ASSERT_TRUE(tabGridButtonVisible,
                             @"Show tab grid button was not visible.");

  // TODO(crbug.com/41442441) For an unknown reason synchronization doesn't work
  // well with tapping on the tabgrid button, and instead triggers the long
  // press gesture recognizer.  Disable this here so the test can be re-enabled.
  ScopedSynchronizationDisabler disabler;
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_longPressWithDuration(base::Milliseconds(50))];
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
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabShareButton()]
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

- (void)waitForAppToIdle {
  GREYWaitForAppToIdle(@"App failed to idle");
}

- (void)openPageInfo {
  [self openToolsMenu];
  [self tapToolsMenuButton:chrome_test_util::SiteInfoDestinationButton()];
}

- (BOOL)dismissContextMenuIfPresent {
  // There is no way to programmatically dismiss the native context menu from
  // application side, so instead tap on the context menu container view if it
  // exists.
  NSError* err = nil;
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_kindOfClassName(
                                              @"_UIContextMenuContainerView"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))
              error:&err];
  return err == nil;
}

- (void)dismissByTappingOnTheWindowOfPopover:(id<GREYMatcher>)matcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher = grey_descendant(matcher);
  id<GREYMatcher> windowMatcher = grey_allOf(classMatcher, parentMatcher, nil);

  // Tap on a point outside of the popover.
  // The way EarlGrey taps doesn't go through the window hierarchy. Because of
  // this, the tap needs to be done in the same window as the popover.
  [[EarlGrey selectElementWithMatcher:windowMatcher]
      performAction:grey_tapAtPoint(CGPointMake(0, 0))];

  // Verify the window is not visible.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:windowMatcher]
        assertWithMatcher:grey_notVisible()
                    error:&error];
    return !error;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 kWaitForUIElementTimeout, condition),
             @"Popover was not dismissed.");
}

- (void)longPressElementOnWebView:(ElementSelector*)selector {
  // Use triggers_context_menu = true as this is really "triggers_browser_menu".
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        selector, true /* menu should appear */)];
}

- (void)triggerEditMenu:(ElementSelector*)selector {
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_notVisible()];
  [self longPressElementOnWebView:selector];

  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
      assertWithMatcher:grey_sufficientlyVisible()
                  error:&error];
  if (error) {
    // If edit is not visible, try to tap the element again.
    // This is possible on inputs when the first long press just selects the
    // input.
    [self longPressElementOnWebView:selector];
    [[EarlGrey selectElementWithMatcher:[EditMenuAppInterface editMenuMatcher]]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

- (void)clearAndDismissSearchBar {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // On iPad, when running on iOS 26+ and building with the iOS 26+ SDK, the
    // search bar is dismissed after clearing the text. The button for clearing
    // text is always displayed.
    if (@available(iOS 26, *)) {
      [[EarlGrey
          selectElementWithMatcher:chrome_test_util::SearchBarClearTextButton()]
          performAction:grey_tap()];
      return;
    }

    // On iPad, when running on iOS < 26 or building with an SDK older than
    // iOS 26, the search bar is cleared and dismissed via the "Cancel" button.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  } else {
    // On iPhone, the search text is cleared and the search bar is dismissed via
    // a single button press.

    // When running on iOS 26+ and building with the iOS 26+ SDK, tap the
    // "Close" button.
    if (@available(iOS 26, *)) {
      [[EarlGrey selectElementWithMatcher:chrome_test_util::CloseButton()]
          performAction:grey_tap()];
      return;
    }

    // When running on iOS < 26 or building with an SDK older than iOS 26, tap
    // the "Cancel" button.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
}

#pragma mark - Private

// Clears all browsing data from the device. This method needs to be called when
// the "Clear Browsing Data" panel is opened.
- (void)selectAllBrowsingDataAndClear {
  // Set 'Time Range' to 'All Time'.
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE))]
      performAction:grey_tap()];

  NSString* timeRange = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME);
  id<GREYMatcher> popupCellMenuItem = grey_allOf(
      grey_not(grey_accessibilityID(kQuickDeletePopUpButtonIdentifier)),
      ContextMenuItemWithAccessibilityLabel(timeRange), nil);
  [[EarlGrey selectElementWithMatcher:popupCellMenuItem]
      performAction:grey_tap()];

  // Tap on the browsing data subpage.
  [ChromeEarlGreyUI tapPrivacyMenuButton:BrowsingDataButtonMatcher()];

  // Check "Saved Passwords" and "Autofill Data" which are unchecked by
  // default.
  [ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:ClearSavedPasswordsButton()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(ClearAutofillButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeSlowInDirection(kGREYDirectionUp)
      onElementWithMatcher:ClearBrowsingDataView()] performAction:grey_tap()];

  // Tap the confirm button to save the prefs.
  [[EarlGrey selectElementWithMatcher:BrowsingDataConfirmButtonMatcher()]
      performAction:grey_tap()];

  // Clear data, and confirm.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Wait until the spinner is cleared, meaning that clear browsing data has
  // finished.
  [self waitForAppToIdle];

  // Reset selection.
  [ChromeEarlGrey resetBrowsingDataPrefs];
}

// Waits for the clear browsing data view to become visible if `isVisible` is
// YES, otherwise waits for it to disappear. If the condition is not met within
// a timeout, a GREYAssert is induced.
- (void)waitForClearBrowsingDataViewVisible:(BOOL)isVisible {
  ConditionBlock condition = ^{
    NSError* error = nil;
    id<GREYMatcher> visibleMatcher =
        isVisible ? grey_sufficientlyVisible() : grey_nil();
    [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
        assertWithMatcher:visibleMatcher
                    error:&error];
    return error == nil;
  };
  NSString* errorMessage = isVisible
                               ? @"Clear browsing data view was not visible"
                               : @"Clear browsing data view was visible";
  bool clearBrowsingDataViewVisibility =
      base::test::ios::WaitUntilConditionOrTimeout(
          kWaitForClearBrowsingDataTimeout, condition);
  EG_TEST_HELPER_ASSERT_TRUE(clearBrowsingDataViewVisibility, errorMessage);
}

- (void)typeTextInOmnibox:(std::string const&)text
            andPressEnter:(BOOL)shouldPressEnter {
  BOOL textHasBeenTypedProperly = NO;
  int numberOfAttemptsPerformed = 0;
  while (!textHasBeenTypedProperly &&
         numberOfAttemptsPerformed <
             kMaxNumberOfAttemptsAtTypingTextInOmnibox) {
    [self focusOmniboxAndReplaceText:base::SysUTF8ToNSString(text)];
    numberOfAttemptsPerformed++;

    // Check that the omnibox contains the typed text.
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(text)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    textHasBeenTypedProperly = error == nil;

    if (!textHasBeenTypedProperly &&
        numberOfAttemptsPerformed < kMaxNumberOfAttemptsAtTypingTextInOmnibox) {
      // Text has not been typed properly. Defocusing the omnibox so a new
      // attempt is possible next round of loop.
      if ([ChromeEarlGrey isIPadIdiom]) {
        id<GREYMatcher> typingShield = grey_accessibilityID(@"Typing Shield");
        [[EarlGrey selectElementWithMatcher:typingShield]
            performAction:grey_tap()];
      } else {
        [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Cancel")]
            performAction:grey_tap()];
      }
    }
  }

  if (textHasBeenTypedProperly && shouldPressEnter) {
    [self pressEnter];
  }

  // Assert the text has been typed properly.
  GREYAssert(textHasBeenTypedProperly,
             @"Failed to type '%s' in the Omnibox after %d attempts.",
             text.c_str(), numberOfAttemptsPerformed);
}

@end
