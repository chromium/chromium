// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// This coordinator encapsulates the integration to ScreenTime, reporting web
// usage and blocking restricted webpages.
@interface ScreenTimeCoordinator : ChromeCoordinator
// The view controller that must be placed above the web view to enable web
// usage reporting. This view controller automatically blocks when the reported
// URL becomes restricted.
@property(nonatomic, readonly) UIViewController* viewController;
@end

#endif  // IOS_CHROME_BROWSER_SCREEN_TIME_UI_BUNDLED_SCREEN_TIME_COORDINATOR_H_
