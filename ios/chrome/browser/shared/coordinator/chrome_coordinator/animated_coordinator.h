// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_ANIMATED_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_ANIMATED_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Class for a ChromeCoordinator that can be stopped with or without
// animated. It is expected that the `stop` method of classes implementing this
// protocol is defined as `[self stopAnimated:NO]`.
// Subclasses should not reimplement `stop`. Instead, they should implement
// `-stopAnimated` which should call `[super stopAnimated]`.
@interface AnimatedCoordinator : ChromeCoordinator

// Same as `ChromeCoordinator`’s stop, but can be animated or not. Must call
// `[super stop]`.
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_CHROME_COORDINATOR_ANIMATED_COORDINATOR_H_
