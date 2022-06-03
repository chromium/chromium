// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class PrivacyCoordinator;

// Delegate that allows to dereference the PrivacyCoordinator.
@protocol PrivacyCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)privacyCoordinatorViewControllerWasRemoved:
    (PrivacyCoordinator*)coordinator;

@end

// The coordinator for the Privacy screen.
@interface PrivacyCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<PrivacyCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_PRIVACY_COORDINATOR_H_
