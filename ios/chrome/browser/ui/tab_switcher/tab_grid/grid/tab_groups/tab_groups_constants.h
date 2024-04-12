// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier for the tab group creation view.
extern NSString* const kCreateTabGroupViewIdentifier;
extern NSString* const kCreateTabGroupTextFieldIdentifier;
extern NSString* const kCreateTabGroupCreateButtonIdentifier;
extern NSString* const kCreateTabGroupCancelButtonIdentifier;

// Timing constants for the animations of the TabGroup presentation/dismissal.
extern const CGFloat kTabGroupPresentationDuration;
extern const CGFloat kTabGroupDismissalDuration;
extern const CGFloat kTabGroupBackgroundElementDurationFactor;

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_TAB_GROUPS_TAB_GROUPS_CONSTANTS_H_
