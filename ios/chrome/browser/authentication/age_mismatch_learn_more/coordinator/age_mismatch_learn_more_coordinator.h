// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_LEARN_MORE_COORDINATOR_AGE_MISMATCH_LEARN_MORE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_LEARN_MORE_COORDINATOR_AGE_MISMATCH_LEARN_MORE_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AgeMismatchLearnMoreCoordinator;

@protocol AgeMismatchLearnMoreCoordinatorDelegate <NSObject>

- (void)ageMismatchLearnMoreCoordinatorWantsToBeStopped:
    (AgeMismatchLearnMoreCoordinator*)coordinator;

@end

// Coordinator to display the Age Mismatch Learn More content in a web view.
@interface AgeMismatchLearnMoreCoordinator : ChromeCoordinator

@property(nonatomic, weak) id<AgeMismatchLearnMoreCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_AGE_MISMATCH_LEARN_MORE_COORDINATOR_AGE_MISMATCH_LEARN_MORE_COORDINATOR_H_
