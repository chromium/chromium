// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_coordinator.h"

#import "ios/chrome/browser/contextual_panel/coordinator/contextual_sheet_presenter.h"
#import "ios/chrome/browser/contextual_panel/coordinator/panel_content_coordinator.h"
#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"

@implementation ContextualSheetCoordinator {
  ContextualSheetViewController* _viewController;

  PanelContentCoordinator* _panelContentCoordinator;
}

- (void)start {
  _viewController = [[ContextualSheetViewController alloc] init];
  _viewController.contextualSheetHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ContextualSheetCommands);

  [self.baseViewController addChildViewController:_viewController];

  [self.presenter insertContextualSheet:_viewController.view];

  [_viewController didMoveToParentViewController:self.baseViewController];

  _panelContentCoordinator = [[PanelContentCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser];
  [_panelContentCoordinator start];

  [_viewController animateAppearance];
}

- (void)stop {
  [_panelContentCoordinator stop];

  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

@end
