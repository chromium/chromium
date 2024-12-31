// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_COORDINATOR_H_

#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

typedef void (^ParentAccessCallbackCompletion)(
    supervised_user::LocalApprovalResult result);

// Coordinator for local website approval, allowing parents to authenticate
// to approve website navigation requests from a supervised user.
// This will be presented within the same browser session where the supervised
// user is signed in.
@interface ParentAccessCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                completion:
                                    (ParentAccessCallbackCompletion)completion
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_COORDINATOR_H_
