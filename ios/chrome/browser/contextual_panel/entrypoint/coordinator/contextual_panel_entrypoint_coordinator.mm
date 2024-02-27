// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/coordinator/contextual_panel_entrypoint_mediator_delegate.h"
#import "ios/chrome/browser/contextual_panel/entrypoint/ui/contextual_panel_entrypoint_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_panel_commands.h"

@interface ContextualPanelEntrypointCoordinator () <
    ContextualPanelCommands,
    ContextualPanelEntrypointMediatorDelegate>

// The mediator for this coordinator.
@property(nonatomic, strong) ContextualPanelEntrypointMediator* mediator;

// The view controller for this coordinator.
@property(nonatomic, strong)
    ContextualPanelEntrypointViewController* viewController;

@end

@implementation ContextualPanelEntrypointCoordinator

- (void)start {
  [super start];
  _viewController = [[ContextualPanelEntrypointViewController alloc] init];

  _mediator = [[ContextualPanelEntrypointMediator alloc] init];
  _mediator.delegate = self;

  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(ContextualPanelCommands)];
}

- (void)stop {
  CHECK(_viewController);
  CHECK(_mediator);

  [super stop];

  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  _viewController.mutator = nil;
  _viewController = nil;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:self];
}

#pragma mark - ContextualPanelCommands

- (void)showContextualPanelEntrypoint {
}

@end
