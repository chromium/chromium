// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_REPOST_FORM_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_REPOST_FORM_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace web {
class WebState;
}

@protocol RepostFormCoordinatorDelegate;

// Creates and manages a repost form dialog that has Continue and Cancel
// buttons.
@interface RepostFormCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<RepostFormCoordinatorDelegate> delegate;

// Initializes a coordinator for displaying an alert on this `viewController`.
// `browser` should be passed as a parameter in the initializer for the
// providing model-layer dependencies and a command dispatcher that can be used
// by the coordinator and its children. `dialogLocation` is a point where the
// repost form dialog should be presented on iPad (in `viewController`'s
// coordinate space). `webState` must not be null and must be owned by the
// caller. `completionHandler` will be called with YES when Continue button is
// tapped and with NO when Cancel button is tapped. `completionHandler` can not
// be null.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            dialogLocation:(CGPoint)dialogLocation
                                  webState:(web::WebState*)webState
                         completionHandler:(void (^)(BOOL))completionHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_ALERT_REPOST_FORM_COORDINATOR_H_
