// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_transition_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"

@implementation TabGroupCoordinator {
  // Mediator for tab groups.
  TabGroupMediator* _mediator;
  // View controller for tab groups.
  TabGroupViewController* _viewController;
  // Transition delegate for the animation to show/hide a Tab Group.
  TabGroupTransitionDelegate* _transitionDelegate;
  // Tab group to display.
  const TabGroup* _tabGroup;
}

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  tabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group coordinator outside the "
         "Tab Groups experiment.";
  CHECK(tabGroup) << "You need to pass a tab group in order to display it.";
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _tabGroup = tabGroup;
  }
  return self;
}

- (UIViewController*)viewController {
  return _viewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  id<TabGroupsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabGroupsCommands);
  _viewController = [[TabGroupViewController alloc]
      initWithHandler:handler
           lightTheme:!self.browser->GetBrowserState()->IsOffTheRecord()
             tabGroup:_tabGroup];

  _mediator = [[TabGroupMediator alloc]
      initWithWebStateList:self.browser->GetWebStateList()
                  tabGroup:_tabGroup
                  consumer:_viewController
              gridConsumer:_viewController.gridViewController];
  _mediator.browser = self.browser;

  _viewController.mutator = _mediator;
  _viewController.gridViewController.mutator = _mediator;

  _viewController.modalPresentationStyle = UIModalPresentationCustom;
  _transitionDelegate = [[TabGroupTransitionDelegate alloc]
      initWithTabGroupViewController:_viewController];
  _viewController.transitioningDelegate = _transitionDelegate;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _mediator = nil;

  // TODO(crbug.com/1501837): Make the hide tab group animation.
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
}

@end
