// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"

#import "ios/chrome/browser/app_bar/coordinator/app_bar_container_mediator.h"
#import "ios/chrome/browser/app_bar/coordinator/app_bar_mediator.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_browser_agent.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/app_bar_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"
#import "ios/chrome/browser/shared/public/commands/guided_tour_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

@interface AppBarCoordinator () <AccountMenuCoordinatorDelegate,
                                 GuidedTourCommands,
                                 AppBarCommands>
@end

@implementation AppBarCoordinator {
  AppBarContainerViewController* _containerViewController;
  AppBarViewController* _viewController;
  AppBarMediator* _mediator;
  AppBarContainerMediator* _containerMediator;
  raw_ptr<Browser> _incognitoBrowser;
  raw_ptr<Browser> _regularBrowser;
  // The account menu coordinator.
  AccountMenuCoordinator* _accountMenuCoordinator;
}

- (instancetype)initWithRegularBrowser:(Browser*)regularBrowser
                      incognitoBrowser:(Browser*)incognitoBrowser {
  self = [super init];
  if (self) {
    _incognitoBrowser = incognitoBrowser;
    _regularBrowser = regularBrowser;
  }
  return self;
}

- (void)start {
  CommandDispatcher* regularDispatcher =
      _regularBrowser->GetCommandDispatcher();
  CommandDispatcher* incognitoDispatcher =
      _incognitoBrowser->GetCommandDispatcher();
  // It is ok to use the regular browser here as the Scene commands are
  // handled by the same object for both modes.
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(regularDispatcher, SceneCommands);
  id<TabGridCommands> tabGridHandler =
      HandlerForProtocol(regularDispatcher, TabGridCommands);
  id<LensCommands> lensHandler =
      HandlerForProtocol(regularDispatcher, LensCommands);
  id<BWGCommands> geminiHandler =
      HandlerForProtocol(regularDispatcher, BWGCommands);

  SceneState* sceneState = _regularBrowser->GetSceneState();

  [regularDispatcher startDispatchingToTarget:self
                                  forProtocol:@protocol(AppBarCommands)];
  [incognitoDispatcher startDispatchingToTarget:self
                                    forProtocol:@protocol(AppBarCommands)];

  _viewController = [[AppBarViewController alloc] init];
  _viewController.sceneHandler = sceneHandler;
  _viewController.tabGridHandler = tabGridHandler;
  _viewController.layoutGuideCenter = LayoutGuideCenterForScene(sceneState);
  _viewController.layoutState = sceneState.layoutState;
  ProfileIOS* profile = _regularBrowser->GetProfile();

  FullscreenController* regularFullscreenController = nullptr;
  FullscreenController* incognitoFullscreenController = nullptr;
  if (!IsFullscreenRefactoringEnabled()) {
    regularFullscreenController =
        FullscreenController::FromBrowser(_regularBrowser);
    incognitoFullscreenController =
        FullscreenController::FromBrowser(_incognitoBrowser);
  }

  BrowserActionFactory* regularActionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:_regularBrowser
             scenario:kMenuScenarioHistogramToolbarMenu];
  BrowserActionFactory* incognitoActionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:_incognitoBrowser
             scenario:kMenuScenarioHistogramToolbarMenu];

  FullscreenBrowserAgent* regularAgent = nullptr;
  FullscreenBrowserAgent* incognitoAgent = nullptr;
  if (IsFullscreenRefactoringEnabled()) {
    regularAgent = FullscreenBrowserAgent::FromBrowser(_regularBrowser);
    incognitoAgent = FullscreenBrowserAgent::FromBrowser(_incognitoBrowser);
  }

  _mediator = [[AppBarMediator alloc]
          initWithRegularWebStateList:_regularBrowser->GetWebStateList()
                incognitoWebStateList:_incognitoBrowser->GetWebStateList()
          regularFullscreenController:regularFullscreenController
        incognitoFullscreenController:incognitoFullscreenController
        regularFullscreenBrowserAgent:regularAgent
      incognitoFullscreenBrowserAgent:incognitoAgent
                 regularActionFactory:regularActionFactory
               incognitoActionFactory:incognitoActionFactory
                          prefService:profile->GetPrefs()
                   templateURLService:ios::TemplateURLServiceFactory::
                                          GetForProfile(
                                              _regularBrowser->GetProfile())
                authenticationService:AuthenticationServiceFactory::
                                          GetForProfile(profile)
                        geminiService:GeminiServiceFactory::GetForProfile(
                                          profile)
                   geminiBrowserAgent:GeminiBrowserAgent::FromBrowser(
                                          _regularBrowser)
                            URLLoader:UrlLoadingBrowserAgent::FromBrowser(
                                          _regularBrowser)
                         tabGridState:sceneState.tabGridState
                       incognitoState:sceneState.incognitoState];
  _mediator.sceneHandler = sceneHandler;
  _mediator.lensHandler = lensHandler;
  _mediator.tabGridHandler = tabGridHandler;
  _mediator.settingsHandler =
      HandlerForProtocol(regularDispatcher, SettingsCommands);
  _mediator.geminiHandler = geminiHandler;
  if (IsFullscreenRefactoringEnabled()) {
    _mediator.regularFullscreenHandler =
        HandlerForProtocol(regularDispatcher, FullscreenCommands);
    _mediator.incognitoFullscreenHandler =
        HandlerForProtocol(incognitoDispatcher, FullscreenCommands);
  }
  _mediator.baseViewController = _viewController;
  _mediator.regularTabGroupsCommands =
      HandlerForProtocol(regularDispatcher, TabGroupsCommands);
  _mediator.incognitoTabGroupsCommands =
      HandlerForProtocol(incognitoDispatcher, TabGroupsCommands);

  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  _containerViewController = [[AppBarContainerViewController alloc] init];
  [_containerViewController setAppBar:_viewController];
  _containerViewController.layoutState =
      _regularBrowser->GetSceneState().layoutState;

  _containerMediator = [[AppBarContainerMediator alloc]
      initWithRegularFullscreenController:regularFullscreenController
            incognitoFullscreenController:incognitoFullscreenController
            regularFullscreenBrowserAgent:regularAgent
          incognitoFullscreenBrowserAgent:incognitoAgent];
  _containerMediator.consumer = _containerViewController;

  if (IsBestOfAppGuidedTourEnabled()) {
    [_regularBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(GuidedTourCommands)];
  }
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_containerMediator disconnect];
  _containerMediator = nil;
  [_regularBrowser->GetCommandDispatcher() stopDispatchingToTarget:self];
  if (_incognitoBrowser) {
    [_incognitoBrowser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }
  _containerViewController.layoutState = nil;
  _containerViewController = nil;
  _viewController.layoutState = nil;
  _viewController = nil;
  _regularBrowser = nullptr;
  _incognitoBrowser = nullptr;
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

#pragma mark AppBarMediatorDelegate

- (void)showAccountMenu:(UIView*)anchorView {
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_regularBrowser
                      anchorView:anchorView
                     accessPoint:AccountMenuAccessPoint::kNewTabPage
                             URL:GURL()];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return _containerViewController;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
  [_mediator setIncognitoWebStateList:incognitoBrowser
                                          ? incognitoBrowser->GetWebStateList()
                                          : nullptr];

  [_mediator setIncognitoActionFactory:
                 incognitoBrowser
                     ? [[BrowserActionFactory alloc]
                           initWithBrowser:incognitoBrowser
                                  scenario:kMenuScenarioHistogramToolbarMenu]
                     : nil];
  CommandDispatcher* incognitoDispatcher =
      incognitoBrowser ? _incognitoBrowser->GetCommandDispatcher() : nil;
  _mediator.incognitoTabGroupsCommands =
      incognitoDispatcher
          ? HandlerForProtocol(incognitoDispatcher, TabGroupsCommands)
          : nil;

  if (incognitoDispatcher) {
    [incognitoDispatcher startDispatchingToTarget:self
                                      forProtocol:@protocol(AppBarCommands)];
  }

  if (IsFullscreenRefactoringEnabled()) {
    FullscreenBrowserAgent* incognitoAgent =
        incognitoBrowser ? FullscreenBrowserAgent::FromBrowser(incognitoBrowser)
                         : nullptr;
    [_mediator setIncognitoFullscreenBrowserAgent:incognitoAgent];
    [_containerMediator setIncognitoFullscreenBrowserAgent:incognitoAgent];
    _mediator.incognitoFullscreenHandler =
        incognitoDispatcher
            ? HandlerForProtocol(incognitoDispatcher, FullscreenCommands)
            : nil;
  } else {
    FullscreenController* incognitoFullscreenController =
        incognitoBrowser ? FullscreenController::FromBrowser(incognitoBrowser)
                         : nullptr;
    [_mediator setIncognitoFullscreenController:incognitoFullscreenController];
    [_containerMediator
        setIncognitoFullscreenController:incognitoFullscreenController];
  }
}

#pragma mark - GuidedTourCommands

- (void)highlightViewInStep:(GuidedTourStep)step {
  if (step == GuidedTourStep::kNTP) {
    [_viewController toggleSpotlightView:YES];
  }
}

- (void)stepCompleted:(GuidedTourStep)step {
  if (step == GuidedTourStep::kNTP) {
    [_viewController toggleSpotlightView:NO];
  }
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  CHECK_EQ(_accountMenuCoordinator, coordinator, base::NotFatalUntil::M140);
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

#pragma mark - AppBarCommands

- (void)showIPHBackgroundWithCentering:(BOOL)centered {
  [_viewController showIPHBackgroundWithCentering:centered];
}

- (void)hideIPHBackground {
  [_viewController hideIPHBackground];
}

@end
