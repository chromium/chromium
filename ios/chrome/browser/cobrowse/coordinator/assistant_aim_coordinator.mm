// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

@interface AssistantAIMCoordinator () <AssistantAIMViewControllerDelegate,
                                       AssistantContainerDelegate>
@end

@implementation AssistantAIMCoordinator {
  AssistantAIMViewController* _viewController;
  AssistantAIMMediator* _mediator;
}

- (void)start {
  _viewController = [[AssistantAIMViewController alloc] init];
  _viewController.delegate = self;

  web::WebState::CreateParams params(self.browser->GetProfile());
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  _mediator =
      [[AssistantAIMMediator alloc] initWithWebState:std::move(webState)];
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  id<AssistantContainerCommands> containerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AssistantContainerCommands);

  [containerHandler showAssistantContainerWithContent:_viewController
                                             delegate:self];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;

  if (_viewController) {
    _viewController = nil;
    if (self.browser) {
      CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
      if ([dispatcher
              dispatchingForProtocol:@protocol(AssistantContainerCommands)]) {
        id<AssistantContainerCommands> containerHandler =
            HandlerForProtocol(dispatcher, AssistantContainerCommands);
        [containerHandler dismissAssistantContainerAnimated:NO completion:nil];
      }
    }
  }
}

#pragma mark - AssistantAIMViewControllerDelegate

- (void)assistantAIMViewControllerDidTapClose:
    (AssistantAIMViewController*)viewController {
  id<AssistantContainerCommands> containerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AssistantContainerCommands);
  [containerHandler dismissAssistantContainerAnimated:YES completion:nil];
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainer:(AssistantContainerViewController*)container
      didDisappearAnimated:(BOOL)animated {
  [self stop];
}

@end
