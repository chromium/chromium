// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_SIGNOUT_ENTERPRISE_SIGNOUT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_SIGNOUT_ENTERPRISE_SIGNOUT_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Delegate for the coordinator.
@protocol EnterpriseSignoutCoordinatorDelegate

// Notifies the delegate that the view has been dismissed.
- (void)enterpriseSignoutCoordinatorDidDismiss;

@end

// Coordinator for Sync alerts.
@interface EnterpriseSignoutCoordinator : ChromeCoordinator

// Delegate for dismissing the coordinator.
@property(nonatomic, weak) id<EnterpriseSignoutCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_ENTERPRISE_SIGNOUT_ENTERPRISE_SIGNOUT_COORDINATOR_H_
