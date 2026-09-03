// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ActuationWorklogViewController;

// Coordinator managing the actuation worklog lifecycle and bridging task
// updates from `ActorService` to the worklog UI.
@interface ActuationWorklogCoordinator : ChromeCoordinator

// The view controller displaying the actuation worklog.
@property(nonatomic, readonly) ActuationWorklogViewController* viewController;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_COORDINATOR_H_
