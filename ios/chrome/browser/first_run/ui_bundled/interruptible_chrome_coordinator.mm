// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interruptible_chrome_coordinator.h"

@implementation InterruptibleChromeCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  if (completion) {
    completion();
  }
}

@end
