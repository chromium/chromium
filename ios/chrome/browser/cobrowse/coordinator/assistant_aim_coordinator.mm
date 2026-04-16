// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import <vector>

#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/model/ios_contextual_tasks_service_factory.h"
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
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/web_state.h"

@interface AssistantAIMCoordinator () <AssistantAIMViewControllerDelegate,
                                       AssistantContainerDelegate,
                                       AssistantAIMMediatorDelegate,
                                       TabGridStateObserver>

// Returns whether the tab grid is currently visible.
- (BOOL)isTabGridVisible;

@end

namespace {

class AssistantAIMUIStateProvider
    : public CobrowseBrowserAgent::UIStateProvider {
 public:
  explicit AssistantAIMUIStateProvider(AssistantAIMCoordinator* coordinator)
      : coordinator_(coordinator) {}

  bool IsTabGridVisible() override { return [coordinator_ isTabGridVisible]; }

 private:
  __weak AssistantAIMCoordinator* coordinator_;
};

}  // namespace

@implementation AssistantAIMCoordinator {
  AssistantAIMViewController* _viewController;
  AssistantAIMMediator* _mediator;
  ComposeboxInputPlateCoordinator* _inputPlateCoordinator;
  ComposeboxModeHolder* _modeHolder;
  std::unique_ptr<AssistantAIMUIStateProvider> _uiStateProvider;
  AssistantContainerDetent _currentDetent;

  // Handler for container related interactions.
  __weak id<AssistantContainerCommands> _containerHandler;
}


- (void)start {
  CHECK(IsAimCobrowseEnabled());
  _currentDetent = AssistantContainerDetent::kMinimized;
  if (self.browser->GetProfile()->IsOffTheRecord()) {
    return;
  }

  [self.browser->GetSceneState().tabGridState addObserver:self];

  CobrowseBrowserAgent* agent = CobrowseBrowserAgent::FromBrowser(self.browser);
  if (agent) {
    _uiStateProvider = std::make_unique<AssistantAIMUIStateProvider>(self);
    agent->SetUIStateProvider(_uiStateProvider.get());
  }

  _viewController = [[AssistantAIMViewController alloc] init];
  _viewController.delegate = self;

  _containerHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         AssistantContainerCommands);

  [_containerHandler showAssistantContainerWithContent:_viewController
                                              delegate:self];

  web::WebState::CreateParams params(self.browser->GetProfile());
  CobrowseContext* context = agent ? agent->GetCobrowseContext() : nil;
  if (!context) {
    context = [CobrowseContext defaultContext];
  }
  contextual_tasks::ContextualTasksService* contextualTasksService = nullptr;
  if (IsCobrowseAimHistoryEnabled()) {
    contextualTasksService = IOSContextualTasksServiceFactory::GetForProfile(
        self.browser->GetProfile());
  }

  _mediator = [[AssistantAIMMediator alloc]
            initWithWebState:web::WebState::Create(params)
                     context:context
            containerHandler:_containerHandler
      contextualTasksService:contextualTasksService];
  _mediator.delegate = self;
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

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

  CobrowseBrowserAgent* agent = CobrowseBrowserAgent::FromBrowser(self.browser);
  if (agent) {
    agent->SetUIStateProvider(nullptr);
  }
  _uiStateProvider.reset();

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

#pragma mark - CobrowseBrowserAgent::UIStateProvider

- (BOOL)isTabGridVisible {
  return self.browser->GetSceneState().tabGridState.tabGridVisible;
}

#pragma mark - TabGridStateObserver

- (void)willEnterTabGrid {
  [self dismissAssistantContainerAnimated:YES];
}

#pragma mark - AssistantAIMViewControllerDelegate

- (void)assistantAIMViewControllerDidTapClose:
    (AssistantAIMViewController*)viewController {
  CobrowseBrowserAgent* browserAgent =
      CobrowseBrowserAgent::FromBrowser(self.browser);
  CHECK(browserAgent);
  browserAgent->SetSessionActive(false);
  [self dismissAssistantContainerAnimated:YES];
}

- (void)assistantAIMViewController:(AssistantAIMViewController*)viewController
       didShowKeyboardWithDuration:(NSTimeInterval)duration
                             curve:(UIViewAnimationCurve)curve {
  [_containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kLarge
                               duration:duration
                                  curve:curve];
}

- (void)assistantAIMViewControllerDidHideKeyboard:
    (AssistantAIMViewController*)viewController {
}

- (void)assistantAIMViewControllerDidRequestEndEditing:
    (AssistantAIMViewController*)viewController {
  [self dismissKeyboard];
}

#pragma mark - Private

- (void)dismissKeyboard {
  [_inputPlateCoordinator endEditing];
}

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
      didDisappearAnimated:(BOOL)animated {
  [self stop];
}

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

- (void)assistantContainer:(AssistantContainerViewController*)container
           didChangeDetent:(AssistantContainerDetent)newDetent {
  _currentDetent = newDetent;
  // Attempt to dismiss the keyboard when the sheet is collapsing.
  if (newDetent == AssistantContainerDetent::kMedium) {
    [self dismissKeyboard];
  }
}

#pragma mark - AssistantAIMMediatorDelegate

- (void)assistantAIMMediatorDidLoadQuery:(AssistantAIMMediator*)mediator {
  [self dismissKeyboard];
}

- (BOOL)assistantContainer:(AssistantContainerViewController*)container
     shouldPauseScrollView:(UIScrollView*)scrollView
                forGesture:(UIGestureRecognizer*)otherGesture {
  const std::vector<AssistantContainerDetent>& detents = container.detents;
  BOOL isInLargestDetent = (_currentDetent == detents.back());

  return [_viewController shouldPauseScrollView:scrollView
                                     forGesture:otherGesture
                              isInLargestDetent:isInLargestDetent];
}

@end
