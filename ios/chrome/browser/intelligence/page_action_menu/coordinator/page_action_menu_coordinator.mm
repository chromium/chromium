// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_coordinator.h"

#import "ios/chrome/browser/intelligence/page_action_menu/coordinator/page_action_menu_mediator.h"
#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/glic_commands.h"

@implementation PageActionMenuCoordinator {
  PageActionMenuViewController* _viewController;
  PageActionMenuMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[PageActionMenuViewController alloc] init];
  _mediator = [[PageActionMenuMediator alloc] init];
  id<GlicCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), GlicCommands);
  _viewController.handler = handler;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  [super start];
}

- (void)stop {
  _viewController = nil;
  _mediator = nil;

  [super stop];
}

@end
