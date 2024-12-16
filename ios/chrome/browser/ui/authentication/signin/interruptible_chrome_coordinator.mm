// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/interruptible_chrome_coordinator.h"

BASE_FEATURE(kIOSInterruptibleCoordinatorAlwaysDismissed,
             "IOSInterruptibleCoordinatorAlwaysDismissed",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSInterruptibleCoordinatorStoppedSynchronously,
             "IOSInterruptibleCoordinatorStoppedSynchronously",
             base::FEATURE_DISABLED_BY_DEFAULT);

SigninCoordinatorInterrupt SynchronousStopAction() {
  if (base::FeatureList::IsEnabled(
          kIOSInterruptibleCoordinatorAlwaysDismissed)) {
    // If the interruption is not synchronous, we must continue to send
    // UIShutdownNoDismiss.
    CHECK(base::FeatureList::IsEnabled(
              kIOSInterruptibleCoordinatorStoppedSynchronously),
          base::NotFatalUntil::M136);
    return SigninCoordinatorInterrupt::DismissWithoutAnimation;
  }
  return SigninCoordinatorInterrupt::UIShutdownNoDismiss;
}

@implementation InterruptibleChromeCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}

@end
