// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_METRICS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_METRICS_H_

namespace overflow_menu {
enum class Destination;
enum class ActionType;
}  // namespace overflow_menu

// Enum for IOS.OverflowMenu.ActionType histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSOverflowMenuActionType {
  kNoScrollNoAction = 0,
  kScrollNoAction = 1,
  kNoScrollAction = 2,
  kScrollAction = 3,
  kMaxValue = kScrollAction,
};

// Enum for IOS.OverflowMenu.SmartSortingStateChange histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSOverflowMenuSmartSortingChange {
  kNewlyEnabled = 0,
  kNewlyDisabled = 1,
  kMaxValue = kNewlyDisabled,
};

// Enum for IOS.OverflowMenu.DestinationsOrderChangedProgrammatically histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSOverflowMenuReorderingReason {
  kErrorBadge = 0,
  kNewBadge = 1,
  kBothBadges = 2,
  kMaxValue = kBothBadges,
};

// Enum for many histograms, corresponding to enums.xml's
// IOSOverflowMenuDestination.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSOverflowMenuDestination {
  kBookmarks = 0,
  kHistory = 1,
  kReadingList = 2,
  kPasswords = 3,
  kDownloads = 4,
  kRecentTabs = 5,
  kSiteInfo = 6,
  kSettings = 7,
  kWhatsNew = 8,
  kSpotlightDebugger = 9,
  kPriceNotifications = 10,
  kMaxValue = kPriceNotifications,
};

// Returns the correct destination histogram enum value for the given
// `destination`.
IOSOverflowMenuDestination HistogramDestinationFromDestination(
    overflow_menu::Destination destination);

// Enum for many histograms, corresponding to enums.xml's
// IOSOverflowMenuAction.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSOverflowMenuAction {
  kReload = 0,
  kNewTab = 1,
  kNewIncognitoTab = 2,
  kNewWindow = 3,
  kFollow = 4,
  kBookmark = 5,
  kReadingList = 6,
  kClearBrowsingData = 7,
  kTranslate = 8,
  kDesktopSite = 9,
  kFindInPage = 10,
  kTextZoom = 11,
  kReportAnIssue = 12,
  kHelp = 13,
  kShareChrome = 14,
  kEditActions = 15,
  kMaxValue = kEditActions,
};

// Returns the correct action histogram enum value for the given `action_type`.
IOSOverflowMenuAction HistogramActionFromActionType(
    overflow_menu::ActionType action_type);

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_METRICS_H_
