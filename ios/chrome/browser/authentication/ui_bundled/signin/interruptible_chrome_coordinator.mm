// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/interruptible_chrome_coordinator.h"

BASE_FEATURE(kIOSInterruptibleCoordinatorAlwaysDismissed,
             "IOSInterruptibleCoordinatorAlwaysDismissed",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSInterruptibleCoordinatorStoppedSynchronously,
             "IOSInterruptibleCoordinatorStoppedSynchronously",
             base::FEATURE_DISABLED_BY_DEFAULT);

BOOL IsInterruptibleCoordinatorStoppedSynchronouslyEnabled() {
  return base::FeatureList::IsEnabled(
      kIOSInterruptibleCoordinatorStoppedSynchronously);
}

BOOL IsInterruptibleCoordinatorAlwaysDismissedEnabled() {
  return base::FeatureList::IsEnabled(
      kIOSInterruptibleCoordinatorAlwaysDismissed);
}

SigninCoordinatorInterrupt SynchronousStopAction() {
  if (IsInterruptibleCoordinatorAlwaysDismissedEnabled()) {
    // If the interruption is not synchronous, we must continue to send
    // UIShutdownNoDismiss.
    CHECK(IsInterruptibleCoordinatorStoppedSynchronouslyEnabled(),
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
