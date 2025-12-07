// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_COORDINATOR_GOOGLE_ONE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_COORDINATOR_GOOGLE_ONE_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/google_one/shared/google_one_entry_point.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol SystemIdentity;

// A coordinator to display Google One management screen.
@interface GoogleOneCoordinator : ChromeCoordinator

// Create a Google One coordinator to present account management related to
// `identity` on `viewController`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:(GoogleOneEntryPoint)entryPoint
                                  identity:(id<SystemIdentity>)identity;

@end

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_COORDINATOR_GOOGLE_ONE_COORDINATOR_H_
