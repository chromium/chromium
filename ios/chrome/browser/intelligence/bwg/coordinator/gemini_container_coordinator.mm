// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_coordinator.h"

#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"

@interface GeminiContainerCoordinator () <AssistantContainerDelegate,
                                          GeminiContainerViewControllerDelegate>
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
  // TODO(crbug.com/535579970): After bottom sheet migration, the startup state
  // can be added to the init params.
  _mediator = [[GeminiContainerMediator alloc]
      initWithBrowser:self.browser
               target:GeminiBrowserAgent::FromBrowser(self.browser)];

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
  _viewController = [[GeminiContainerViewController alloc]
      initWithGeminiViewController:geminiViewController];
  _viewController.delegate = self;

  _containerHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         AssistantContainerCommands);
  [_containerHandler showAssistantContainerWithContent:_viewController
                                              delegate:self];
}

- (void)dismissWithCompletion:(void (^)(void))completion {
  [_containerHandler dismissAssistantContainerAnimated:YES
                                            completion:completion];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
  _containerHandler = nil;
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainerDidUpdateDetentHeights:
    (AssistantContainerViewController*)container {
  NSInteger collapsedHeight =
      [container heightForDetent:AssistantContainerDetent::kMinimized];
  NSInteger extendedHeight =
      [container heightForDetent:AssistantContainerDetent::kMedium];

  if (collapsedHeight > 0 && extendedHeight > 0) {
    ios::provider::UpdateDetentHeights(collapsedHeight, extendedHeight);
  }
}

#pragma mark - GeminiContainerViewControllerDelegate

- (void)geminiContainerViewController:
            (GeminiContainerViewController*)viewController
          didShowKeyboardWithDuration:(NSTimeInterval)duration
                                curve:(UIViewAnimationCurve)curve {
  [_containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kLarge
                               duration:duration
                                  curve:curve];
}

#pragma mark - Private

- (void)setSessionCommandHandlers {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _mediator.gatewayManager.sessionHandler.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  _mediator.gatewayManager.sessionHandler.geminiHandler =
      HandlerForProtocol(dispatcher, GeminiCommands);
}

@end
