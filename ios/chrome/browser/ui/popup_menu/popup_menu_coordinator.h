// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_long_press_delegate.h"

@class BubblePresenter;
@protocol PopupMenuUIUpdating;

// Coordinator for the popup menu, handling the commands.
@interface PopupMenuCoordinator : ChromeCoordinator<PopupMenuLongPressDelegate>

// UI updater.
@property(nonatomic, weak) id<PopupMenuUIUpdating> UIUpdater;
// Bubble view presenter for the incognito tip.
@property(nonatomic, weak) BubblePresenter* bubblePresenter;

// Returns whether this coordinator is showing a popup menu.
- (BOOL)isShowingPopupMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_COORDINATOR_H_
