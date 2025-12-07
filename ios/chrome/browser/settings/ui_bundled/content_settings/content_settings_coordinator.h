// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_CONTENT_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_CONTENT_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ContentSettingsCoordinator;

// Delegate that allows to dereference the ContentSettingsCoordinator.
@protocol ContentSettingsCoordinatorDelegate

// Called when the view controller is removed from navigation controller.
- (void)contentSettingsCoordinatorViewControllerWasRemoved:
    (ContentSettingsCoordinator*)coordinator;

@end

// Coordinator to display the screen that allows the user to change content
// settings like blocking popups.
@interface ContentSettingsCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<ContentSettingsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_CONTENT_SETTINGS_COORDINATOR_H_
