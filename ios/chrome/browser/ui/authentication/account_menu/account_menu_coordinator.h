// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol AccountMenuCoordinatorDelegate;

// Coordinator to display the fast account menu view controller.
@interface AccountMenuCoordinator : ChromeCoordinator

// Clicked view, used to anchor the menu to it when using
// UIModalPresentationPopover mode.
@property(nonatomic, strong) UIView* anchorView;

// Delegate for the coordinator.
@property(nonatomic, weak) id<AccountMenuCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_H_
