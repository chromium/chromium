// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/supervised_user/ui/parent_access_view_controller_delegate.h"

class ChromeAccountManagerService;
class SystemIdentityManager;

@protocol SystemIdentity;

namespace signin {
class IdentityManager;
}  // namespace signin

// Mediator for ParentAccessCoordinator.
@interface ParentAccessMediator : NSObject <ParentAccessViewControllerDelegate>

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
            systemIdentityManager:(SystemIdentityManager*)systemIdentityManager
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SUPERVISED_USER_COORDINATOR_PARENT_ACCESS_MEDIATOR_H_
