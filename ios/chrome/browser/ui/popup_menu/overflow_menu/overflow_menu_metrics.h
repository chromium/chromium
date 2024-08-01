// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_METRICS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_METRICS_H_

#import <Foundation/Foundation.h>

#import "base/containers/enum_set.h"

namespace overflow_menu {
enum class Destination;
enum class ActionType;
}  // namespace overflow_menu
@class OverflowMenuAction;
@class OverflowMenuDestination;

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
// LINT.IfChange(destination)
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
// LINT.ThenChange(overflow_menu_constants.h:destination)

// Returns the correct destination histogram enum value for the given
// `destination`.
IOSOverflowMenuDestination HistogramDestinationFromDestination(
    overflow_menu::Destination destination);

// Enum for many histograms, corresponding to enums.xml's
// IOSOverflowMenuAction.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(actionType)
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
  kLensOverlay = 16,
  kMaxValue = kLensOverlay,
};
// LINT.ThenChange(overflow_menu_constants.h:actionType)

// Returns the correct action histogram enum value for the given `action_type`.
IOSOverflowMenuAction HistogramActionFromActionType(
    overflow_menu::ActionType action_type);

// Field indexes for bitmask storing these values in a histogram.
// These values are saved to logs, so entries should not be renumbered and
// numeric values should never be reused. Also, the total number of items should
// not go above 64.
enum class DestinationsCustomizationEventFields {
  kSmartSortingTurnedOff = 0,
  kMinValue = kSmartSortingTurnedOff,
  kSmartSortingTurnedOn = 1,
  kSmartSortingIsOn = 2,
  kDestinationWasRemoved = 3,
  kDestinationWasAdded = 4,
  kDestinationWasReordered = 5,
  kMaxValue = kDestinationWasReordered,
};

// Stats to log for a single destination customization event.
using DestinationsCustomizationEvent =
    base::EnumSet<DestinationsCustomizationEventFields,
                  DestinationsCustomizationEventFields::kMinValue,
                  DestinationsCustomizationEventFields::kMaxValue>;

// Records the logging event for a given destination customization event.
void RecordDestinationsCustomizationEvent(DestinationsCustomizationEvent event);

// Field indexes for bitmask storing these values in a histogram.
// These values are saved to logs, so entries should not be renumbered and
// numeric values should never be reused. Also, the total number of items should
// not go above 64.
enum class ActionsCustomizationEventFields {
  kActionWasRemoved = 0,
  kMinValue = kActionWasRemoved,
  kActionWasAdded = 1,
  kActionWasReordered = 2,
  kMaxValue = kActionWasReordered,
};

// Stats to log for a single action customization event.
using ActionsCustomizationEvent =
    base::EnumSet<ActionsCustomizationEventFields,
                  ActionsCustomizationEventFields::kMinValue,
                  ActionsCustomizationEventFields::kMaxValue>;

// Records the logging event for a given action customization event.
void RecordActionsCustomizationEvent(ActionsCustomizationEvent event);

// Field indexes for bitmask storing these values in a histogram.
// These values are saved to logs, so entries should not be renumbered and
// numeric values should never be reused. Also, the total number of items should
// not go above 64.
enum class OverflowMenuVisitedEventFields {
  kUserSelectedDestination = 0,
  kMinValue = kUserSelectedDestination,
  kUserSelectedAction = 1,
  kUserScrolledVertically = 2,
  kUserScrolledHorizontally = 3,
  kUserStartedCustomization = 4,
  kUserCancelledCustomization = 5,
  kUserCustomizedDestinations = 6,
  kUserCustomizedActions = 7,
  kMaxValue = kUserCustomizedActions,
};

// Stats to log for a single overflow menu visit
using OverflowMenuVisitedEvent =
    base::EnumSet<OverflowMenuVisitedEventFields,
                  OverflowMenuVisitedEventFields::kMinValue,
                  OverflowMenuVisitedEventFields::kMaxValue>;

void RecordOverflowMenuVisitedEvent(OverflowMenuVisitedEvent event);

// Returns whether the given `destination` was initially visible in the list of
// visible `destinations`, given that `visibleDestinationCount` destinations are
// visible at a time.
bool DestinationWasInitiallyVisible(
    overflow_menu::Destination destination,
    NSArray<OverflowMenuDestination*>* destinations,
    int visibleDestinationCount);

// Returns whether the given `action` was initially visible in the list of
// visible `actions`, given that `visibleActionCount` actions are visible at a
// time.
bool ActionWasInitiallyVisible(overflow_menu::ActionType actionType,
                               NSArray<OverflowMenuAction*>* actions,
                               int visibleActionCount);

// Returns whether the given `item` was initially visible in the list of visible
// `items`, given that `visibleItemCount` items are visible at a time.
// Exposed mostly for testing. The two above methods are more convenient.
bool ItemWasInitiallyVisible(int item,
                             NSArray<NSNumber*>* items,
                             int visibleItemCount);

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_OVERFLOW_MENU_METRICS_H_
