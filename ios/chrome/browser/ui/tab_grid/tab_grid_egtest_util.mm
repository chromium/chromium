// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Identifer for cell at given |index| in the tab grid.
NSString* IdentifierForCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}
}  // namespace

namespace chrome_test_util {

id<GREYMatcher> TabGridOpenButton() {
  if (IsRegularXRegularSizeClass()) {
    return ButtonWithAccessibilityLabelId(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER);
  } else {
    return ButtonWithAccessibilityLabelId(IDS_IOS_TOOLBAR_SHOW_TABS);
  }
}

id<GREYMatcher> TabGridDoneButton() {
  return grey_allOf(grey_accessibilityID(kTabGridDoneButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridCloseAllButton() {
  return grey_allOf(grey_accessibilityID(kTabGridCloseAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridUndoCloseAllButton() {
  return grey_allOf(grey_accessibilityID(kTabGridUndoCloseAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridSelectShowHistoryCell() {
  return grey_allOf(grey_accessibilityID(
                        kRecentTabsShowFullHistoryCellAccessibilityIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridRegularTabsEmptyStateView() {
  return grey_allOf(
      grey_accessibilityID(kTabGridRegularTabsEmptyStateIdentifier),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridNewTabButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_TAB_GRID_CREATE_NEW_TAB),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridNewIncognitoTabButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridOpenTabsPanelButton() {
  return grey_accessibilityID(kTabGridRegularTabsPageButtonIdentifier);
}

id<GREYMatcher> TabGridIncognitoTabsPanelButton() {
  return grey_accessibilityID(kTabGridIncognitoTabsPageButtonIdentifier);
}

id<GREYMatcher> TabGridOtherDevicesPanelButton() {
  return grey_accessibilityID(kTabGridRemoteTabsPageButtonIdentifier);
}

id<GREYMatcher> TabGridCellAtIndex(unsigned int index) {
  return grey_allOf(grey_accessibilityID(IdentifierForCellAtIndex(index)),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridCloseButtonForCellAtIndex(unsigned int index) {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(IdentifierForCellAtIndex(index))),
      grey_accessibilityID(kGridCellCloseButtonIdentifier),
      grey_sufficientlyVisible(), nil);
}

}  // namespace chrome_test_util
