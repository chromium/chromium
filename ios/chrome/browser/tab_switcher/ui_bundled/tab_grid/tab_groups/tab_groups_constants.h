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
// Height of the buttons.
extern const CGFloat kTabGroupButtonHeight;

// Accessibility identifiers for the Recent Activity view.
extern NSString* const kTabGroupRecentActivityIdentifier;

// Accessibility identifiers for the tab groups panel in Tab Grid.
extern NSString* const kTabGroupsPanelIdentifier;

// Accessibility identifiers of cells in the tab groups panel.
extern NSString* const kTabGroupsPanelOutOfDateMessageCellIdentifier;
extern NSString* const kTabGroupsPanelNotificationCellIdentifierPrefix;
extern NSString* const kTabGroupsPanelCellIdentifierPrefix;

// Accessibility identifiers for the tab groups panel out-of-date message cell.
extern NSString* const kTabGroupsPanelUpdateOutOfDateMessageIdentifier;
extern NSString* const kTabGroupsPanelCloseOutOfDateMessageIdentifier;

// Accessibility identifier for the tab groups panel notification cell.
extern NSString* const kTabGroupsPanelCloseNotificationIdentifier;

// Accessibility identifier of the shared tab groups user education screen.
extern NSString* const kSharedTabGroupUserEducationAccessibilityIdentifier;
// Name of the pref storing whether the user education has been shown or not.
extern NSString* const kSharedTabGroupUserEducationShownOnceKey;

// Accessibility identifier prefix of a cell in the recent activity.
extern NSString* const kRecentActivityLogCellIdentifierPrefix;

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_
