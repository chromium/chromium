// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INTERRUPTIBLE_CHROME_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INTERRUPTIBLE_CHROME_COORDINATOR_H_

#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Interface for a ChromeCoordinator that can be interrupted without following
// the conventional flow.
@interface InterruptibleChromeCoordinator : ChromeCoordinator

// Interrupt the coordinator to immediately tear down the views it manages.
// Depending on `animated`, the interruption may be done with/without an
// animation, or without dismissing the views when the interruption is for
// shutdown (e.g., tearing down the scene). `completion` is called
// synchronously. Simply calls `completion` if the method is not overridden.
// TODO(crbug.com/381444097): Remove the completion parameter.
- (void)interruptAnimated:(BOOL)animated completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_INTERRUPTIBLE_CHROME_COORDINATOR_H_
