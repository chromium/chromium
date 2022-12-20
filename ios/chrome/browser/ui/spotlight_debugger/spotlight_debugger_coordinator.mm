// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_coordinator.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_view_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SpotlightDebuggerCoordinator ()

@property(nonatomic, strong) SpotlightDebuggerViewController* viewController;

@end

@implementation SpotlightDebuggerCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  self.viewController = [[SpotlightDebuggerViewController alloc] init];
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];

  [self.baseViewController presentViewController:navController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;

  [super stop];
}
@end
