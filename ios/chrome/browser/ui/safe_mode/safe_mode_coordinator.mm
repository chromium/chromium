// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/safe_mode/safe_mode_coordinator.h"

#include "base/logging.h"
#include "ios/chrome/browser/crash_report/crash_loop_detection_util.h"
#import "ios/chrome/browser/ui/safe_mode/safe_mode_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kStartupCrashLoopThreshold = 2;
}

@interface SafeModeCoordinator ()<SafeModeViewControllerDelegate>
@end

@implementation SafeModeCoordinator {
  __weak UIWindow* _window;
}

@synthesize delegate = _delegate;

#pragma mark - Public class methods

+ (BOOL)shouldStart {
  // Check whether there appears to be a startup crash loop. If not, don't look
  // at anything else.
  if (crash_util::GetFailedStartupAttemptCount() < kStartupCrashLoopThreshold)
    return NO;

  return [SafeModeViewController hasSuggestions];
}

#pragma mark - ChromeCoordinator implementation

- (void)start {
  // Create the SafeModeViewController and make it the root view controller for
  // the window. The window has ownership of it and will dispose of it when
  // another view controller is made root.
  //
  // General note: Safe mode should be safe; it should not depend on other
  // objects being created. Be extremely conservative when adding code to this
  // method.
  SafeModeViewController* viewController =
      [[SafeModeViewController alloc] initWithDelegate:self];
  [self.window setRootViewController:viewController];

  // Reset the crash count; the user may change something based on the recovery
  // UI that will fix the crash, and having the next launch start in recovery
  // mode would be strange.
  crash_util::ResetFailedStartupAttemptCount();
}

// Override of ChildCoordinators method, which is not supported in this class.
- (MutableCoordinatorArray*)childCoordinators {
  NOTREACHED() << "Do not add child coordinators to SafeModeCoordinator.";
  return nil;
}

#pragma mark - SafeModeViewControllerDelegate implementation

- (void)startBrowserFromSafeMode {
  [self.delegate coordinatorDidExitSafeMode:self];
}

@end
