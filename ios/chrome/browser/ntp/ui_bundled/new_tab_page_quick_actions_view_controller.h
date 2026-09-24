// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class LayoutGuideCenter;
@protocol NewTabPageShortcutsHandler;

// The user interface for the quick actions on NTP, displayed just below the
// NTP omnibox if eligible.
@interface NewTabPageQuickActionsViewController : UIViewController

// The layout guide center for referencing views.
@property(nonatomic, weak) LayoutGuideCenter* layoutGuideCenter;

// Handles the actions for the NTP shortcuts, like Lens or voice search.
@property(nonatomic, weak) id<NewTabPageShortcutsHandler> NTPShortcutsHandler;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_QUICK_ACTIONS_VIEW_CONTROLLER_H_
