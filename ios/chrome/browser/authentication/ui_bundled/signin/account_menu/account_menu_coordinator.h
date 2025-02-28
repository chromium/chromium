// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

@protocol AccountMenuCoordinatorDelegate;

// Coordinator to display the fast account menu view controller.
@interface AccountMenuCoordinator : SigninCoordinator

// `anchorView`: Clicked view, used to anchor the menu to it when using
// UIModalPresentationPopover mode.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                anchorView:(UIView*)anchorView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_ACCOUNT_MENU_ACCOUNT_MENU_COORDINATOR_H_
