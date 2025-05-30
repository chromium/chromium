// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifiers for the tab group creation view.
extern NSString* const kCreateTabGroupViewIdentifier;
extern NSString* const kCreateTabGroupTextFieldIdentifier;
extern NSString* const kCreateTabGroupTextFieldClearButtonIdentifier;
extern NSString* const kCreateTabGroupCreateButtonIdentifier;
extern NSString* const kCreateTabGroupCancelButtonIdentifier;

// Accessibility identifiers for the tab group view.
extern NSString* const kTabGroupViewIdentifier;
extern NSString* const kTabGroupViewTitleIdentifier;
extern NSString* const kTabGroupNewTabButtonIdentifier;
extern NSString* const kTabGroupOverflowMenuButtonIdentifier;
extern NSString* const kTabGroupCloseButtonIdentifier;
extern NSString* const kTabGroupFacePileButtonIdentifier;

// Color for the button background.
UIColor* TabGroupViewButtonBackgroundColor();

// Accessibility identifiers for the Recent Activity view.
extern NSString* const kTabGroupRecentActivityIdentifier;

// Accessibility identifiers for the Tab Groups panel in Tab Grid.
extern NSString* const kTabGroupsPanelIdentifier;

// Accessibility identifier prefix of cells in the Tab Groups panel.
extern NSString* const kTabGroupsPanelNotificationCellIdentifierPrefix;
extern NSString* const kTabGroupsPanelCellIdentifierPrefix;

// Accessibility identifier for the Tab Groups panel notification cell.
extern NSString* const kTabGroupsPanelCloseNotificationIdentifier;

// Accessibility identifier of the Shared Tab Groups user education screen.
extern NSString* const kSharedTabGroupUserEducationAccessibilityIdentifier;
// Name of the pref storing whether the user education has been shown or not.
extern NSString* const kSharedTabGroupUserEducationShownOnceKey;

// Accessibility identifier prefix of a cell in the recent activity.
extern NSString* const kRecentActivityLogCellIdentifierPrefix;

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_
