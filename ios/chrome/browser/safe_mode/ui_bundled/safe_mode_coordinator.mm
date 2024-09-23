// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_coordinator.h"

#import <ostream>

#import "base/notreached.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_view_controller.h"

namespace {
const int kStartupCrashLoopThreshold = 3;
}

@interface SafeModeCoordinator ()<SafeModeViewControllerDelegate>
@property(weak, nonatomic, readonly) UIWindow* window;
@end

@implementation SafeModeCoordinator

@synthesize delegate = _delegate;

#pragma mark - Public class methods

- (instancetype)initWithWindow:(UIWindow*)window {
  if ((self = [super initWithBaseViewController:nil browser:nullptr])) {
    _window = window;
  }
  return self;
}

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
  DCHECK(self.window);
  [self.window setRootViewController:viewController];

  // Reset the crash count; the user may change something based on the recovery
  // UI that will fix the crash, and having the next launch start in recovery
  // mode would be strange.
  crash_util::ResetFailedStartupAttemptCount();
}

// Override of ChildCoordinators method, which is not supported in this class.
- (MutableCoordinatorArray*)childCoordinators {
  NOTREACHED_IN_MIGRATION()
      << "Do not add child coordinators to SafeModeCoordinator.";
  return nil;
}

#pragma mark - SafeModeViewControllerDelegate implementation

- (void)startBrowserFromSafeMode {
  [self.delegate coordinatorDidExitSafeMode:self];
}

@end
