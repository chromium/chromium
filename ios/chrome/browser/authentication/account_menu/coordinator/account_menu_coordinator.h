// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_COORDINATOR_ACCOUNT_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_COORDINATOR_ACCOUNT_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class AccountMenuAccessPoint;
@protocol AccountMenuCoordinatorDelegate;
class GURL;

// Coordinator to display the fast account menu view controller.
@interface AccountMenuCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<AccountMenuCoordinatorDelegate> delegate;

// `anchorView`: Clicked view, used to anchor the menu to it when using
// UIModalPresentationPopover mode.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                anchorView:(UIView*)anchorView
                               accessPoint:(AccountMenuAccessPoint)accessPoint
                                       URL:(const GURL&)url
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_ACCOUNT_MENU_COORDINATOR_ACCOUNT_MENU_COORDINATOR_H_
