// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_ONE_COORDINATOR_GOOGLE_ONE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_GOOGLE_ONE_COORDINATOR_GOOGLE_ONE_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/google_one/shared/google_one_entry_point.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class GURL;
@protocol SystemIdentity;

// A coordinator to display Google One management screen.
// This class respects the `kRestrictAccountsToPatterns` policy and expects
// `identity` to correspond to an account on the device that matches
// `kRestrictAccountsToPatterns`.
@interface GoogleOneCoordinator : ChromeCoordinator

// Create a Google One coordinator to present account management related to
// `identity` on `viewController`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:(GoogleOneEntryPoint)entryPoint
                                  identity:(id<SystemIdentity>)identity;

// Create a Google One coordinator to present account management triggered
// via deep link with `inputURL`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:(GoogleOneEntryPoint)entryPoint
                                  inputURL:(const GURL&)inputURL;

@end

#endif  // IOS_CHROME_BROWSER_GOOGLE_ONE_COORDINATOR_GOOGLE_ONE_COORDINATOR_H_
