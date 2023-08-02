// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_coordinator.h"

#import "base/check_op.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/reading_list_spotlight_manager.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_swift.h"
#import "ios/chrome/browser/ui/spotlight_debugger/spotlight_debugger_view_controller.h"

@interface SpotlightDebuggerCoordinator () <
    SpotlightDebuggerViewControllerDelegate>

@property(nonatomic, strong) SpotlightDebuggerViewController* viewController;

@end

@implementation SpotlightDebuggerCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  self.viewController = [[SpotlightDebuggerViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.bookmarksManager = [BookmarksSpotlightManager
      bookmarksSpotlightManagerWithBrowserState:self.browser
                                                    ->GetBrowserState()];

  self.viewController.readingListSpotlightManager = [ReadingListSpotlightManager
      readingListSpotlightManagerWithBrowserState:self.browser
                                                      ->GetBrowserState()];

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

#pragma mark - SpotlightDebuggerViewControllerDelegate

- (void)showAllItems {
  SpotlightDebuggerAllItemsViewController* allItemsViewController =
      [[SpotlightDebuggerAllItemsViewController alloc] init];
  [self.viewController.navigationController
      pushViewController:allItemsViewController
                animated:YES];
}

@end
