// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_coordinator.h"

#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

@interface GeminiContainerCoordinator ()
@end

@implementation GeminiContainerCoordinator {
  // Startup state used to initialize the Gemini content.
  GeminiStartupState* _startupState;
  // The view controller displaying the Gemini content.
  GeminiContainerViewController* _viewController;
  // Command dispatcher handler to manage the assistant container.
  __weak id<AssistantContainerCommands> _containerHandler;
  // Mediator for the Gemini container.
  GeminiContainerMediator* _mediator;
  // The zero-state suggestions view controller.
  GeminiZeroStateViewController* _geminiZeroStateViewController;
  // Child coordinator managing the actuation worklog.
  ActuationWorklogCoordinator* _actuationWorklogCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              startupState:(GeminiStartupState*)startupState {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _startupState = startupState;
  }
  return self;
}

- (void)start {
  actor::ActorService* actorService = nullptr;
  if (IsGeminiActorEnabled()) {
    actorService =
        actor::ActorServiceFactory::GetForProfile(self.browser->GetProfile());
  }

  // TODO(crbug.com/535579970): After bottom sheet migration, the startup state
  // can be added to the init params.
  _mediator = [[GeminiContainerMediator alloc]
      initWithBrowser:self.browser
         actorService:actorService
         eventHandler:GeminiBrowserAgent::FromBrowser(self.browser)];
  _containerHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         AssistantContainerCommands);
  _mediator.containerHandler = _containerHandler;

  [self setSessionCommandHandlers];

  GeminiConfiguration* config = [_mediator
      createGeminiConfigurationForActiveWebState:_startupState
                              baseViewController:self.baseViewController];

  // TODO(crbug.com/522834798): Add all the applicable logic from
  // StartGeminiFlow, PresentFloaty and InvokeFloaty before presenting the
  // container view.
  // TODO(crbug.com/535968300): Move floaty request to the mediator.
  UIViewController* geminiViewController =
      ios::provider::GetFloatyViewControllerWithConfiguration(config);

  ActuationWorklogViewController* worklogViewController = nil;
  if (IsGeminiActorEnabled()) {
    _actuationWorklogCoordinator = [[ActuationWorklogCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser];
    [_actuationWorklogCoordinator start];
    worklogViewController = _actuationWorklogCoordinator.viewController;
  }

  _viewController = [[GeminiContainerViewController alloc]
      initWithGeminiViewController:geminiViewController
             worklogViewController:worklogViewController];
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

  // Initialize and attach GeminiZeroStateViewController.
  _geminiZeroStateViewController = [[GeminiZeroStateViewController alloc] init];
  _geminiZeroStateViewController.mutator = _mediator;
  _viewController.zeroStateViewController = _geminiZeroStateViewController;
  _mediator.zeroStateConsumer = _geminiZeroStateViewController;

  [_containerHandler showAssistantContainerWithContent:_viewController
                                              delegate:_mediator];

  // Calling connect will result in setting the initial detent
  // which only works after assistant container is presenting.
  [_mediator connect];
}

- (void)dismissWithCompletion:(void (^)(void))completion {
  [_containerHandler dismissAssistantContainerAnimated:YES
                                            completion:completion];
}

- (void)minimize {
  [_containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized];
}

- (void)stop {
  if (IsGeminiActorEnabled()) {
    [_containerHandler setAssistantContainerMinimizedDetentHeight:
                           kAssistantContainerMinimizedDetentHeight];
    [_actuationWorklogCoordinator stop];
    _actuationWorklogCoordinator = nil;
  }
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _containerHandler = nil;
  _geminiZeroStateViewController = nil;
}

#pragma mark - Private

- (void)setSessionCommandHandlers {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _mediator.gatewayManager.sessionHandler.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  id<GeminiCommands> geminiHandler =
      HandlerForProtocol(dispatcher, GeminiCommands);
  _mediator.gatewayManager.sessionHandler.geminiHandler = geminiHandler;
  _mediator.geminiHandler = geminiHandler;
}

@end
