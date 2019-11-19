// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/tab_usage_recorder_test_util.h"

#import <EarlGrey/EarlGrey.h>
#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/testing/nserror_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// The delay to wait for an element to appear before tapping on it.
const NSTimeInterval kWaitElementTimeout = 3;

}  // namespace

namespace tab_usage_recorder_test_util {

bool OpenNewIncognitoTabUsingUIAndEvictMainTabs() {
  int nb_incognito_tab = [ChromeEarlGrey incognitoTabCount];
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> new_incognito_tab_button_matcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:new_incognito_tab_button_matcher]
      performAction:grey_tap()];
  [ChromeEarlGrey waitForIncognitoTabCount:(nb_incognito_tab + 1)];
  bool success = WaitUntilConditionOrTimeout(kWaitElementTimeout, ^{
    return [ChromeEarlGrey isIncognitoMode];
  });
  if (!success) {
    return false;
  }

  [ChromeEarlGrey evictOtherTabModelTabs];
  return true;
}

void SwitchToNormalMode() {
  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Switching to normal mode is only allowed from Incognito.");

  // Enter the tab grid to switch modes.
  [ChromeEarlGrey showTabSwitcher];

  // Switch modes and exit the tab grid.
  TabModel* model = chrome_test_util::GetMainController()
                        .interfaceProvider.mainInterface.tabModel;
  const int tab_index = model.webStateList->active_index();
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::TabGridOpenTabsPanelButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridCellAtIndex(
                                          tab_index)] performAction:grey_tap()];

  BOOL success = NO;
  // Turn off synchronization of GREYAssert to test the pending states.
  {
    ScopedSynchronizationDisabler disabler;
    success = WaitUntilConditionOrTimeout(kWaitElementTimeout, ^{
      return ![ChromeEarlGrey isIncognitoMode];
    });
  }

  if (!success) {
    // TODO(crbug.com/951600): Avoid asserting directly unless the test fails,
    // due to timing issues.
    GREYFail(@"Failed to switch to normal mode.");
  }
}

}  // namespace tab_usage_recorder_test_util
