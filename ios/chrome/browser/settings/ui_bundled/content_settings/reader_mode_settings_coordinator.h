// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_READER_MODE_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_READER_MODE_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ReaderModeSettingsCoordinator;

// Delegate for ReaderModeSettingsCoordinator.
@protocol ReaderModeSettingsCoordinatorDelegate

// Called when the coordinator is stopped.
- (void)readerModeSettingsCoordinatorDidRemove:
    (ReaderModeSettingsCoordinator*)coordinator;

@end

// Coordinator for Reading Mode settings.
@interface ReaderModeSettingsCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<ReaderModeSettingsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_READER_MODE_SETTINGS_COORDINATOR_H_
