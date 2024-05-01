// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_coordinator.h"

#import "ios/chrome/browser/contextual_panel/ui/panel_content_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation PanelContentCoordinator {
  // The view controller managed by this coordinator.
  PanelContentViewController* _viewController;
}

- (void)start {
  _viewController = [[PanelContentViewController alloc] init];
  _viewController.contextualSheetCommandHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ContextualSheetCommands);

  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  AddSameConstraints(self.baseViewController.view, _viewController.view);

  [_viewController didMoveToParentViewController:self.baseViewController];
}

@end
