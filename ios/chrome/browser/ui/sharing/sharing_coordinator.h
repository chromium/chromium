// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SHARING_SHARING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SHARING_SHARING_COORDINATOR_H_

#include <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ActivityServiceCommands;
@class SharingParams;
class Browser;

// Coordinator of sharing scenarios. Its default scenario is to share the
// current tab's URL.
@interface SharingCoordinator : ChromeCoordinator

// Creates a coordinator configured to share the current tab's URL using the
// base `viewController`, a `browser`, `params` with all the necessary values
// to drive the scenario, and an `originView` from which the scenario was
// triggered. This initializer also uses the `originView`'s bounds to position
// the activity view popover on iPad.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(SharingParams*)params
                                originView:(UIView*)originView;

// Creates a coordinator configured to share the URLs specified in `params`.
// This initializer uses `barButtonItem` to position the activity view popover
// on iPad.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(SharingParams*)params
                                    anchor:(UIBarButtonItem*)barButtonItem;

// Creates a coordinator configured to share the current tab's URL using the
// base `viewController`, a `browser`, `params` with all the necessary values
// to drive the scenario. If `barButtonItem` is non-null, it will be used
// to present the activity view popover on iPad. Otherwise, `originView` and
// `originRect` will be used to position the activity view popover on iPad.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(SharingParams*)params
                                originView:(UIView*)originView
                                originRect:(CGRect)originRect
                                    anchor:(UIBarButtonItem*)barButtonItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// If there is a download currently happening, this cancels it and triggers a
// new coordinator to be created.
- (void)cancelIfNecessaryAndCreateNewCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_SHARING_SHARING_COORDINATOR_H_
