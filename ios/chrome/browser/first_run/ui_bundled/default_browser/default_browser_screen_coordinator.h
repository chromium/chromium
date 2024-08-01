// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@protocol FirstRunScreenDelegate;

// Coordinator to present the default browser screen.
@interface DefaultBrowserScreenCoordinator
    : ChromeCoordinator <PromoStyleViewControllerDelegate>

// Initializes a DefaultBrowserScreenCoordinator with `navigationController`,
// `browser`, and `delegate`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:
                                            (id<FirstRunScreenDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_DEFAULT_BROWSER_DEFAULT_BROWSER_SCREEN_COORDINATOR_H_
