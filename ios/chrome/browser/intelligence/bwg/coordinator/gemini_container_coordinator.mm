// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_coordinator.h"

#import <set>
#import <vector>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "base/not_fatal_until.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_worklog_coordinator.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service.h"
#import "ios/chrome/browser/intelligence/actor/model/actor_service_factory.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator.h"
#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_container_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_gateway_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_state_change_handler.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_session_handler.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/gemini_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

@interface GeminiContainerCoordinator () <GeminiContainerMediatorDelegate>
@end

@implementation GeminiContainerCoordinator {
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

- (void)start {
  CHECK(_startupState, base::NotFatalUntil::M160);
  GeminiBrowserAgent* geminiBrowserAgent =
      GeminiBrowserAgent::FromBrowser(self.browser);
  // TODO(crbug.com/571095367): Move source of truth back to coordinator.
  if (geminiBrowserAgent && geminiBrowserAgent->is_floaty_invoked()) {
    CHECK(_mediator, base::NotFatalUntil::M160);
    [_mediator updateWithStartupState:_startupState];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self dismissGeminiFromOtherWindowsWithCompletion:^{
    [weakSelf startContainer];
  }];
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
  if (self.browser) {
    if (GeminiBrowserAgent* geminiBrowserAgent =
            GeminiBrowserAgent::FromBrowser(self.browser)) {
      geminiBrowserAgent->SetContainerInvoked(false);
    }
  }
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
  _startupState = nil;
}

#pragma mark - GeminiContainerMediatorDelegate

- (void)geminiContainerMediator:(GeminiContainerMediator*)mediator
    didStartActuationTaskWithID:(actor::ActorTaskId)taskID {
  [_actuationWorklogCoordinator startObservingTaskWithID:taskID];
}

- (void)geminiContainerMediator:(GeminiContainerMediator*)mediator
     didStopActuationTaskWithID:(actor::ActorTaskId)taskID {
  [_actuationWorklogCoordinator stopObservingTask];
}

#pragma mark - Private

// Starts and presents the Gemini container.
- (void)startContainer {
  actor::ActorService* actorService = nullptr;
  if (IsGeminiActorEnabled()) {
    actorService =
        actor::ActorServiceFactory::GetForProfile(self.browser->GetProfile());
  }

  GeminiBrowserAgent* geminiBrowserAgent =
      GeminiBrowserAgent::FromBrowser(self.browser);
  CHECK(!_mediator, base::NotFatalUntil::M160);
  // TODO(crbug.com/535579970): After bottom sheet migration, the startup state
  // can be added to the init params.
  // TODO(crbug.com/571295640): Don't pass the browser instance to mediator.
  _mediator = [[GeminiContainerMediator alloc]
            initWithBrowser:self.browser
               actorService:actorService
      authenticationService:AuthenticationServiceFactory::GetForProfile(
                                self.browser->GetProfile())
               eventHandler:geminiBrowserAgent];
  _mediator.delegate = self;
  _mediator.startupState = _startupState;
  // TODO(crbug.com/537730178): Delegate the permission prompt request up to
  // a delegate protocol implemented by GeminiContainerCoordinator, which will
  // present the UIAlertController using its own baseViewController.
  [_mediator.gatewayManager.pageStateChangeHandler
      setBaseViewController:self.baseViewController];

  [self setSessionCommandHandlers];

  ActuationWorklogViewController* worklogViewController = nil;
  if (IsGeminiActorEnabled()) {
    _actuationWorklogCoordinator = [[ActuationWorklogCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.browser];
    [_actuationWorklogCoordinator start];
    worklogViewController = _actuationWorklogCoordinator.viewController;
  }

  _viewController = [[GeminiContainerViewController alloc]
      initWithWorklogViewController:worklogViewController];
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;

  // Initialize and attach GeminiZeroStateViewController.
  _geminiZeroStateViewController = [[GeminiZeroStateViewController alloc] init];
  _geminiZeroStateViewController.mutator = _mediator;
  _viewController.zeroStateViewController = _geminiZeroStateViewController;
  _mediator.zeroStateConsumer = _geminiZeroStateViewController;

  [_containerHandler showAssistantContainerWithContent:_viewController
                                              delegate:_mediator];
  if (geminiBrowserAgent) {
    geminiBrowserAgent->SetContainerInvoked(true);
  }

  // Calling connect will result in setting the initial detent
  // which only works after assistant container is presenting.
  [_mediator connect];
}

// Configures the command handlers on `_mediator` and its session handler.
- (void)setSessionCommandHandlers {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _mediator.gatewayManager.sessionHandler.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  id<GeminiCommands> geminiHandler =
      HandlerForProtocol(dispatcher, GeminiCommands);
  _mediator.gatewayManager.sessionHandler.geminiHandler = geminiHandler;
  _mediator.geminiHandler = geminiHandler;

  _containerHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         AssistantContainerCommands);
  _mediator.containerHandler = _containerHandler;
}

// Dismisses Gemini from all other windows and executes `completion`.
- (void)dismissGeminiFromOtherWindowsWithCompletion:
    (ProceduralBlock)completion {
  // Collect all browsers (excluding the current one) for all profiles.
  std::vector<base::WeakPtr<Browser>> otherBrowsers;
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
    const std::set<Browser*>& browsers =
        browserList->BrowsersOfType(BrowserList::BrowserType::kRegular);
    for (Browser* browser : browsers) {
      if (browser == self.browser) {
        continue;
      }
      otherBrowsers.push_back(browser->AsWeakPtr());
    }
  }

  if (otherBrowsers.empty()) {
    if (completion) {
      completion();
    }
    return;
  }

  // Gate the completion behind this barrier closure which executes it when all
  // other browsers have dismissed their Gemini sessions.
  base::RepeatingClosure barrier =
      base::BarrierClosure(otherBrowsers.size(), base::BindOnce(completion));

  // Dismiss Gemini in all the other browsers for all profiles.
  for (base::WeakPtr<Browser> browser : otherBrowsers) {
    if (!browser) {
      barrier.Run();
      continue;
    }
    id<GeminiCommands> geminiHandler =
        HandlerForProtocol(browser->GetCommandDispatcher(), GeminiCommands);
    [geminiHandler
        dismissGeminiFlowWithCompletion:base::CallbackToBlock(barrier)];
  }
}

@end
