// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_H_

#import "base/time/time.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol IdleTimeoutConfirmationCoordinatorDelegate;

@interface IdleTimeoutConfirmationCoordinator : ChromeCoordinator

// Delegate for the coordinator. Can be a parent coordinator that owns this
// coordinator or a scene agent.
@property(nonatomic, weak) id<IdleTimeoutConfirmationCoordinatorDelegate>
    delegate;

// The timestamp when idle timeout was triggered. This is used to calculate how
// many seconds the confirmation dialog should shown for.
@property(nonatomic, assign) base::Time triggerTime;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_UI_BUNDLED_IDLE_IDLE_TIMEOUT_CONFIRMATION_COORDINATOR_H_
