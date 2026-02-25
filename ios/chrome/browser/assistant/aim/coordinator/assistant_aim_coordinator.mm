// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"

#import "ios/chrome/browser/assistant/aim/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

@interface AssistantAIMCoordinator () <AssistantContainerDelegate>
@end

@implementation AssistantAIMCoordinator {
  AssistantAIMViewController* _viewController;
}

- (void)start {
  _viewController = [[AssistantAIMViewController alloc] init];

  id<AssistantContainerCommands> containerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AssistantContainerCommands);

  [containerHandler showAssistantContainerWithContent:_viewController
                                             delegate:self];
}

- (void)stop {
  if (_viewController) {
    if (self.browser) {
      CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
      if ([dispatcher
              dispatchingForProtocol:@protocol(AssistantContainerCommands)]) {
        id<AssistantContainerCommands> containerHandler =
            HandlerForProtocol(dispatcher, AssistantContainerCommands);
        [containerHandler dismissAssistantContainerAnimated:NO completion:nil];
      }
    }
    _viewController = nil;
  }
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainer:(AssistantContainerViewController*)container
      didDisappearAnimated:(BOOL)animated {
  _viewController = nil;
  [self stop];
}

@end
