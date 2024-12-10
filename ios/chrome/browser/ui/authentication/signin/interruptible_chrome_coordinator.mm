// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/interruptible_chrome_coordinator.h"

BASE_FEATURE(kIOSInterruptibleCoordinatorStoppedSynchronously,
             "InterruptibleCoordinatorStoppedSynchronously",
             base::FEATURE_DISABLED_BY_DEFAULT);

@implementation InterruptibleChromeCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}

@end
