// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERRUPTIBLE_CHROME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERRUPTIBLE_CHROME_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

// Interface for a ChromeCoordinator that can be interrupted without following
// the conventional flow.
@interface InterruptibleChromeCoordinator : ChromeCoordinator

// Interrupt the coordinator to immediately tear down the views it manages.
// Dependending on `action`, the interruption may be done asynchronously
// with/without an animation, or without dismissing the views when the
// interruption is for shutdown (e.g., tearing down the scene). `completion` is
// called when the interruption is done. Simply calls `completion` if the
// method is not overridden.
- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_INTERRUPTIBLE_CHROME_COORDINATOR_H_
