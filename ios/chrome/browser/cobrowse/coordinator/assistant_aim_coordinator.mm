// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_coordinator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/web_state.h"

@interface AssistantAIMCoordinator () <AssistantAIMViewControllerDelegate,
                                       AssistantContainerDelegate,
                                       TabGridStateObserver>
@end

@implementation AssistantAIMCoordinator {
  AssistantAIMViewController* _viewController;
  AssistantAIMMediator* _mediator;
  ComposeboxInputPlateCoordinator* _inputPlateCoordinator;
  ComposeboxModeHolder* _modeHolder;
  CobrowseContext* _context;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   context:(CobrowseContext*)context {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _context = context;
  }
  return self;
}

- (void)start {
  [self.browser->GetSceneState().tabGridState addObserver:self];

  _viewController = [[AssistantAIMViewController alloc] init];
  _viewController.delegate = self;

  web::WebState::CreateParams params(self.browser->GetProfile());
  _mediator = [[AssistantAIMMediator alloc]
      initWithWebState:web::WebState::Create(params)
               context:_context];
  _mediator.consumer = _viewController;

  id<AssistantContainerCommands> containerHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), AssistantContainerCommands);

  [containerHandler showAssistantContainerWithContent:_viewController
                                             delegate:self];

  _modeHolder = [[ComposeboxModeHolder alloc] init];
  ComposeboxTheme* theme = [[ComposeboxTheme alloc]
      initWithInputPlatePosition:ComposeboxInputPlatePosition::kBottom
                       incognito:NO
                           isNTP:NO];
  _inputPlateCoordinator = [[ComposeboxInputPlateCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                      entrypoint:ComposeboxEntrypoint::kCobrowse
                           query:nil
                       URLLoader:_mediator
                           theme:theme
                      modeHolder:_modeHolder];
  [_inputPlateCoordinator start];

  [_viewController
      addInputViewController:_inputPlateCoordinator.inputViewController];
}

- (void)stop {
  [self.browser->GetSceneState().tabGridState removeObserver:self];

  [_mediator disconnect];
  _mediator = nil;

  [_inputPlateCoordinator stop];
  _inputPlateCoordinator = nil;
  _modeHolder = nil;

  if (_viewController) {
    _viewController = nil;
    [self dismissAssistantContainerAnimated:NO];
  }
}

#pragma mark - TabGridStateObserver

- (void)willEnterTabGrid {
  [self dismissAssistantContainerAnimated:YES];
}

#pragma mark - AssistantAIMViewControllerDelegate

- (void)assistantAIMViewControllerDidTapClose:
    (AssistantAIMViewController*)viewController {
  [self dismissAssistantContainerAnimated:YES];
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainer:(AssistantContainerViewController*)container
      didDisappearAnimated:(BOOL)animated {
  [self stop];
}

#pragma mark - Private

// Dismisses the assistant container safely.
- (void)dismissAssistantContainerAnimated:(BOOL)animated {
  if (self.browser) {
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    if ([dispatcher
            dispatchingForProtocol:@protocol(AssistantContainerCommands)]) {
      id<AssistantContainerCommands> containerHandler =
          HandlerForProtocol(dispatcher, AssistantContainerCommands);
      [containerHandler dismissAssistantContainerAnimated:animated
                                               completion:nil];
    }
  }
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainer:(AssistantContainerViewController*)container
    didUpdateExpandPercentage:(CGFloat)percentage {
  [_viewController adjustForContainerOpenPercentage:percentage];
}

- (void)assistantContainer:(AssistantContainerViewController*)container
    animateAlongsideTransitionToPercentage:(CGFloat)percentage {
  // NOTE: This API is already called in a animation block so no need to
  // animate.
  [_viewController adjustForContainerOpenPercentage:percentage];
}

@end
