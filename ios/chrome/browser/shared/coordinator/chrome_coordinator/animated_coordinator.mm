// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"

@implementation AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  [super stop];
}

#pragma mark - ChromeCoordinator

- (void)stop {
  [self stopAnimated:NO];
}

@end
