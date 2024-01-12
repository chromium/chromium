// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"

@implementation CreateTabGroupCoordinator {
  // Mediator for tab groups creation.
  CreateTabGroupMediator* _mediator;
  // View controller for tab groups creation.
  CreateTabGroupViewController* _viewController;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups experiment.";
  return [super initWithBaseViewController:viewController browser:browser];
}

- (void)start {
  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  _viewController =
      [[CreateTabGroupViewController alloc] initWithHandler:handler];
  _mediator = [[CreateTabGroupMediator alloc] init];

  // TODO(crbug.com/1501837): Add the create tab group animation.
  _viewController.modalPresentationStyle =
      UIModalPresentationOverCurrentContext;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _mediator = nil;

  // TODO(crbug.com/1501837): Make the created tab group animation.
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
}

@end
