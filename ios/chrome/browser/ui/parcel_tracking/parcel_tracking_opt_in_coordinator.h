// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller_delegate.h"
#import "ios/web/public/annotations/custom_text_checking_result.h"
#import "ios/web/public/web_state.h"

// Coordinator that manages the parcel tracking opt-in half sheet presentation.
@interface ParcelTrackingOptInCoordinator
    : ChromeCoordinator <ParcelTrackingOptInViewControllerDelegate>

// Creates a coordinator that uses `viewController`, `browser`, `webState`, and
// list of parcels `parcels`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  webState:(web::WebState*)webState
                                   parcels:(NSArray<CustomTextCheckingResult*>*)
                                               parcels
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_PARCEL_TRACKING_PARCEL_TRACKING_OPT_IN_COORDINATOR_H_
