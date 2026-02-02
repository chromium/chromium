// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_COORDINATOR_QUICK_DELETE_OTHER_DATA_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_COORDINATOR_QUICK_DELETE_OTHER_DATA_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol QuickDeleteOtherDataCommands;

// Coordinator for "Quick Delete Other Data" page.
@interface QuickDeleteOtherDataCoordinator : ChromeCoordinator

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Handler for QuickDeleteOtherDataCommands.
@property(nonatomic, weak) id<QuickDeleteOtherDataCommands>
    quickDeleteOtherDataHandler;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_OTHER_DATA_COORDINATOR_QUICK_DELETE_OTHER_DATA_COORDINATOR_H_
