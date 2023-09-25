// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PasswordSharingFirstRunCoordinatorDelegate;

// Presents the first run experience view for password sharing explaining
// details of the feature and providing a learn more link.
@interface PasswordSharingFirstRunCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Stops this coordinator and calls `completion` on view controller dismissal.
- (void)stopWithCompletion:(ProceduralBlock)completion;

// Delegate handling coordinator dismissal.
@property(nonatomic, weak) id<PasswordSharingFirstRunCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_PASSWORD_SHARING_FIRST_RUN_COORDINATOR_H_
