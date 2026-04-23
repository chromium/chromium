// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_COORDINATOR_H_

#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ComposeboxMenuCoordinator;

// Delegate for events of `ComposeboxMenuCoordinator`.
@protocol ComposeboxMenuCoordinatorDelegate <NSObject>

// Called when the menu presentation finishes.
- (void)composeboxMenuCoordinatorDidDismissMenu:
    (ComposeboxMenuCoordinator*)composeboxMenuCoordinator;

@end

// Coordinator for the composebox menu on the New Tab Page.
@interface ComposeboxMenuCoordinator : ChromeCoordinator

// The delegate for this coordinator.
@property(nonatomic, weak) id<ComposeboxMenuCoordinatorDelegate> delegate;

// Creates a coordinator that uses `viewController` and `browser` with the given
// entrypoint.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entrypoint:(ComposeboxEntrypoint)entrypoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_COORDINATOR_H_
