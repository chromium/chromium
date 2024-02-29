// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_commands.h"

@interface ContextualPanelEntrypointCoordinator () <
    ContextualPanelEntrypointMediatorDelegate>

// The mediator for this coordinator.
@property(nonatomic, strong) ContextualPanelEntrypointMediator* mediator;

@end

@implementation ContextualPanelEntrypointCoordinator

- (void)start {
  [super start];
  _viewController = [[ContextualPanelEntrypointViewController alloc] init];

  _mediator = [[ContextualPanelEntrypointMediator alloc]
      initWithBrowserAgent:ContextualPanelBrowserAgent::FromBrowser(
                               self.browser)];
  _mediator.delegate = self;

  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:_mediator
                           forProtocol:@protocol(ContextualPanelCommands)];
}

- (void)stop {
  CHECK(_viewController);
  CHECK(_mediator);

  [super stop];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:_mediator];

  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;

  _viewController.mutator = nil;
  _viewController = nil;
}

@end
