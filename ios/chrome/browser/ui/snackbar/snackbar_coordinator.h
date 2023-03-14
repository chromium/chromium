// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SNACKBAR_SNACKBAR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SNACKBAR_SNACKBAR_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator_delegate.h"

// Coordinator that handles commands to show snackbars.
@interface SnackbarCoordinator : ChromeCoordinator

// Initializer for a coordinator for `request`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  delegate:
                                      (id<SnackbarCoordinatorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SNACKBAR_SNACKBAR_COORDINATOR_H_
