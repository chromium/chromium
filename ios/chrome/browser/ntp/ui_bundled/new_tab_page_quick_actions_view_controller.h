// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_VIEW_CONTROLLER_H_
#import <UIKit/UIKit.h>

@protocol NewTabPageShortcutsHandler;

// The user interface for the quick actions on NTP, displayed just below the
// header view, for certain variations when MIA takes over the available
// fakebox real estate
@interface NewTabPageQuickActionsViewController : UIViewController

// The button to open a new incognito tab.
@property(nonatomic, readonly) UIButton* incognitoButton;

// The button to open Lens.
@property(nonatomic, readonly) UIButton* lensButton;

// The button to open voice search.
@property(nonatomic, readonly) UIButton* voiceSearchButton;

// The button to open AIM.
@property(nonatomic, readonly) UIButton* aimButton;

// Handles the actions for the NTP shortcuts, like Lens or voice search.
@property(nonatomic, weak) id<NewTabPageShortcutsHandler> NTPShortcutsHandler;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_VIEW_CONTROLLER_H_
