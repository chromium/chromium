// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_coordinator.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"
#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_breadcrumbs_view_controller.h"
#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_url_view_controller.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_browser_agent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_util.h"
#import "ios/chrome/browser/cobrowse/model/ios_contextual_tasks_service_factory.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_input_plate_coordinator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_mode_holder.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"
#import "ios/chrome/browser/composebox/public/composebox_theme.h"
#import "ios/chrome/browser/composebox/public/features.h"
#import "ios/chrome/browser/metrics/model/activity_reporter.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/tabs/model/tab_helper_filter.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface AssistantAIMCoordinator () <AIMSRPDebuggerURLViewControllerDelegate,
                                       AssistantAIMMediatorDelegate,
                                       AssistantAIMViewControllerDelegate,
                                       AssistantContainerDelegate>

// Block to execute when the 'Undo' snackbar dismisses.
@property(nonatomic, strong) ProceduralBlock undoSnackbarDismissCompletion;

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
  BOOL _isHiding;

  // Whether the coordinator is currently in the middle of stopping.
  BOOL _isStopping;

  // Handler for container related interactions.
  __weak id<AssistantContainerCommands> _containerHandler;
  ActivityReporter* _activityReporter;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  CHECK(IsAimCobrowseEligible(browser->GetProfile()));
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _activityReporter =
        [[ActivityReporter alloc] initWithDomain:ActivityReportDomainCobrowse];
  }
  return self;
}

- (void)start {
  [self startInMinimizedState:NO];
}

- (void)startInMinimizedState:(BOOL)shouldStartInMinimized {
  if (self.browser->GetProfile()->IsOffTheRecord()) {
    return;
  }
  [_activityReporter reportActive];

  CobrowseBrowserAgent* agent = CobrowseBrowserAgent::FromBrowser(self.browser);
  if (agent) {
    _uiStateProvider = std::make_unique<AssistantAIMUIStateProvider>(self);
    agent->SetUIStateProvider(_uiStateProvider.get());
  }

  _viewController = [[AssistantAIMViewController alloc] init];
  _viewController.delegate = self;

  _containerHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                         AssistantContainerCommands);

  contextual_tasks::ContextualTasksService* contextualTasksService =
      IOSContextualTasksServiceFactory::GetForProfile(
          self.browser->GetProfile());

  web::WebState::CreateParams params(self.browser->GetProfile());
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  AttachTabHelpers(webState.get(), TabHelperFilter::kAssistantAim);

  _mediator = [[AssistantAIMMediator alloc]
            initWithWebState:std::move(webState)
        cobrowseBrowserAgent:agent
            containerHandler:_containerHandler
      contextualTasksService:contextualTasksService
                   URLLoader:UrlLoadingBrowserAgent::FromBrowser(self.browser)
       authenticationService:AuthenticationServiceFactory::GetForProfile(
                                 self.browser->GetProfile())];

  _mediator.delegate = self;
  _mediator.sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;

  _modeHolder = [[ComposeboxModeHolder alloc] init];
  ComposeboxTheme* theme = [[ComposeboxTheme alloc]
      initWithInputPlatePosition:ComposeboxInputPlatePosition::kBottom
                       incognito:NO
                           isNTP:NO];
  ComposeboxFocusParams* focusParams = [[ComposeboxFocusParams alloc]
      initWithEntrypoint:ComposeboxEntrypoint::kCobrowse];
  _inputPlateCoordinator = [[ComposeboxInputPlateCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                     focusParams:focusParams
                       URLLoader:_mediator
                           theme:theme
                      modeHolder:_modeHolder];
  [_inputPlateCoordinator start];

  [_viewController
      addInputViewController:_inputPlateCoordinator.inputViewController];

  [self dismissSnackbars];

  BOOL showInMinimizedState =
      shouldStartInMinimized ||
      base::FeatureList::IsEnabled(kAssistantAimMinimizedState);
  AssistantContainerDetent targetDetent =
      showInMinimizedState ? AssistantContainerDetent::kMinimized
                           : AssistantContainerDetent::kMedium;
  _currentDetent = targetDetent;

  // This must be called AFTER the view controller and its children (like the
  // input plate) are fully set up. This is because the initial layout and
  // percentage updates need to be applied to the fully constructed content.
  // Moving it earlier caused the minimized state to not be correctly applied to
  // the content, leading to wrong UI.
  [_containerHandler showAssistantContainerWithContent:_viewController
                                              delegate:self];

  [_containerHandler
      animateAssistantContainerToDetent:targetDetent
                               duration:0
                                  curve:UIViewAnimationCurveEaseInOut];
}

- (void)stop {
  if (_isStopping) {
    return;
  }
  _isStopping = YES;
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
    [self dismissAssistantContainerAnimated:NO completion:nil];
  }
  [_activityReporter reportInactive];
  _activityReporter = nil;
  _isStopping = NO;
}

- (void)setVisible:(BOOL)visible {
  [self setVisible:visible inMinimizedState:NO];
}

- (void)setVisible:(BOOL)visible inMinimizedState:(BOOL)minimized {
  if (visible) {
    [self dismissSnackbars];
    if (_viewController) {
      [_mediator updateContext];
      if (minimized) {
        _currentDetent = AssistantContainerDetent::kMinimized;
      }

      AssistantContainerDetent targetDetent = _currentDetent;
      [_containerHandler showAssistantContainerWithContent:_viewController
                                                  delegate:self];
      // Restore `_currentDetent` in case `showAssistantContainerWithContent:`
      // triggered intermediate layout passes that incorrectly reset it.
      _currentDetent = targetDetent;

      [_activityReporter reportActive];

      [_containerHandler
          animateAssistantContainerToDetent:_currentDetent
                                   duration:kSheetDetentAnimationDuration
                                      curve:UIViewAnimationCurveEaseInOut];
    }
  } else {
    _isHiding = YES;
    [self dismissAssistantContainerAnimated:YES completion:nil];
    [_activityReporter reportInactive];
  }
}

#pragma mark - CobrowseBrowserAgent::UIStateProvider

- (BOOL)isTabGridVisible {
  return self.browser->GetSceneState().tabGridState.tabGridVisible;
}

#pragma mark - AssistantAIMViewControllerDelegate

- (void)assistantAIMViewControllerDidTapClose:
    (AssistantAIMViewController*)viewController {
  [_mediator endSession];
  // Initially the assistant is only hidden, the actual closing happens after
  // the snackbar dismisses and the undo window elapses.
  _isHiding = YES;
  __weak __typeof(self) weakSelf = self;
  [self dismissAssistantContainerAnimated:YES
                               completion:^{
                                 [weakSelf showUndoSnackbar];
                               }];
}

- (void)assistantAIMViewController:(AssistantAIMViewController*)viewController
       didShowKeyboardWithDuration:(NSTimeInterval)duration
                             curve:(UIViewAnimationCurve)curve {
  // Only expand the assistant sheet if the focused field (main responder) is
  // inside the main view.
  UIView* responder = base::apple::ObjCCast<UIView>(GetFirstResponder());
  if (!responder.window) {
    return;
  }

  UIView* containerView = _viewController.view;
  UIView* relevantView = responder;

  while (relevantView) {
    if ([relevantView isDescendantOfView:containerView]) {
      [_containerHandler
          animateAssistantContainerToDetent:AssistantContainerDetent::kLarge
                                   duration:duration
                                      curve:curve];
      return;
    }

    relevantView = relevantView.superview;
  }
}

- (void)assistantAIMViewControllerDidHideKeyboard:
    (AssistantAIMViewController*)viewController {
}

- (void)assistantAIMViewControllerDidRequestEndEditing:
    (AssistantAIMViewController*)viewController {
  [self dismissKeyboard];
}

- (void)assistantAIMViewControllerDidChangeTraits:
    (AssistantAIMViewController*)viewController {
  if (IsIPhoneLandscapeLayout(viewController.traitCollection)) {
    [_containerHandler
        setAssistantContainerDetents:{
                                         AssistantContainerDetent::kMinimized,
                                         AssistantContainerDetent::kLarge,
    }];
    return;
  }
  [_containerHandler
      setAssistantContainerDetents:{
                                       AssistantContainerDetent::kMinimized,
                                       AssistantContainerDetent::kMedium,
                                       AssistantContainerDetent::kLarge,
  }];
}

#pragma mark - Private

// Minimizes the cobrowse container and opens the specified URL.
- (void)minimizeAndOpenURL:(const GURL&)URL {
  [_containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized
                               duration:kSheetDetentAnimationDuration
                                  curve:UIViewAnimationCurveEaseInOut];
  UrlLoadParams params = UrlLoadParams::InCurrentTab(URL);
  UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
}

- (void)dismissKeyboard {
  [_inputPlateCoordinator endEditing];
}

// Dismisses the assistant container safely.
- (void)dismissAssistantContainerAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion {
  if (!self.browser) {
    if (completion) {
      completion();
    }
    return;
  }

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  if ([dispatcher
          dispatchingForProtocol:@protocol(AssistantContainerCommands)]) {
    id<AssistantContainerCommands> containerHandler =
        HandlerForProtocol(dispatcher, AssistantContainerCommands);
    [containerHandler dismissAssistantContainerAnimated:animated
                                             completion:completion];
  }
}

// Closes the assistant.
- (void)closeAssistant {
  if (!self.browser || _isStopping) {
    return;
  }
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler closeAssistant];
}

// Reveals the assistant.
- (void)revealAssistant {
  if (!self.browser) {
    return;
  }
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler revealAssistant];
}

// Shows the undo snackbar with a confirmation message.
//
// While the snackbar is shown the assistant is hidden. If the user presses
// "undo" the assistant is revealed, otherwise it is permanently closed.
- (void)showUndoSnackbar {
  __weak __typeof(self) weakSelf = self;
  __block BOOL didUndo = NO;
  SnackbarMessage* message = [[SnackbarMessage alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_AIM_CLOSE_SNACKBAR_TITLE)];

  message.action = [[SnackbarMessageAction alloc] init];
  message.action.title =
      l10n_util::GetNSString(IDS_IOS_AIM_SNACKBAR_UNDO_BUTTON);
  // Use the helpers for revealing and closing instead of capturing the scene
  // handler explicitly.
  // During browser shutdown, the coordinator's stop sequence immediately
  // triggers all active snackbar completion handlers. Because the scene handler
  // is already deregistered from SceneCommands by this point, calling it
  // directly causes an unrecognized selector exception and a subsequent crash.
  // See crbug.com/525452659 for more details.
  message.action.handler = ^{
    didUndo = YES;
    [weakSelf revealAssistant];
  };

  self.undoSnackbarDismissCompletion = ^{
    if (!didUndo) {
      [weakSelf closeAssistant];
    }
  };
  message.completionHandler = ^(BOOL success) {
    if (weakSelf.undoSnackbarDismissCompletion) {
      weakSelf.undoSnackbarDismissCompletion();
      weakSelf.undoSnackbarDismissCompletion = nil;
    }
  };

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), SnackbarCommands)
      showSnackbarMessage:message];
}

// Dismisses the presented snackbars without triggering the elapsed time side
// effects.
- (void)dismissSnackbars {
  self.undoSnackbarDismissCompletion = nil;
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), SnackbarCommands)
      dismissAllSnackbars];
}

#pragma mark - AssistantContainerDelegate

- (void)assistantContainer:(AssistantContainerViewController*)container
      didDisappearAnimated:(BOOL)animated {
  if (_isHiding || _isStopping) {
    _isHiding = NO;
    return;
  }
  id<SceneCommands> sceneCommands =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [sceneCommands closeAssistant];
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
  if (newDetent == AssistantContainerDetent::kMedium ||
      newDetent == AssistantContainerDetent::kMinimized) {
    [self dismissKeyboard];
  }
}

- (void)assistantContainerDidRequestDismissal:
    (AssistantContainerViewController*)container {
  [_mediator endSession];
  [self dismissAssistantContainerAnimated:YES completion:nil];
}

#pragma mark - AssistantAIMMediatorDelegate

- (void)assistantAIMMediatorDidLoadQuery:(AssistantAIMMediator*)mediator {
  [self dismissKeyboard];
}

- (void)assistantAIMMediatorDidStartNewThread:(AssistantAIMMediator*)mediator {
  [self dismissKeyboard];
}

- (void)assistantAIMMediatorDidFocusFromMinimized:
    (AssistantAIMMediator*)mediator {
  [_inputPlateCoordinator focusComposebox];
}

- (void)assistantAIMMediator:(AssistantAIMMediator*)mediator
    didReceiveContextLibraryWebpageSignalWithURL:(const GURL&)url
                                           title:(NSString*)title {
  [_inputPlateCoordinator processContextLibraryWebpageSignalWithURL:url
                                                              title:title];
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

- (BOOL)assistantContainer:(AssistantContainerViewController*)container
    shouldInterceptPanGesture:(UIPanGestureRecognizer*)gesture {
  return [_viewController shouldInterceptPanGesture:gesture];
}

#pragma mark - AssistantAIMViewControllerDelegate

- (void)assistantAIMViewControllerDidRequestSRPLogs:
    (AssistantAIMViewController*)viewController {
  NSArray<AimSRPDebuggerEvent*>* events = _mediator.debugEvents;
  AimSRPDebuggerBreadcrumbsViewController* logsVC =
      [[AimSRPDebuggerBreadcrumbsViewController alloc] initWithEvents:events];
  UINavigationController* navController =
      [[UINavigationController alloc] initWithRootViewController:logsVC];
  [_viewController presentViewController:navController
                                animated:YES
                              completion:nil];
}

- (void)assistantAIMViewControllerDidRequestLoadedURL:
    (AssistantAIMViewController*)viewController {
  AIMSRPDebuggerURLViewController* URLVC =
      [[AIMSRPDebuggerURLViewController alloc] initWithURL:_mediator.loadedURL];
  URLVC.delegate = self;
  UINavigationController* navController =
      [[UINavigationController alloc] initWithRootViewController:URLVC];
  [_viewController presentViewController:navController
                                animated:YES
                              completion:nil];
}

- (void)assistantAIMViewControllerDidTapMyActivity:
    (AssistantAIMViewController*)viewController {
  [self minimizeAndOpenURL:GURL(kMyActivityURL)];
}

- (void)assistantAIMViewControllerDidTapHelp:
    (AssistantAIMViewController*)viewController {
  [self minimizeAndOpenURL:GURL(kAssistantAIMHelpCenterURL)];
}

#pragma mark - AIMSRPDebuggerURLViewControllerDelegate

- (void)debuggerURLViewController:
            (AIMSRPDebuggerURLViewController*)viewController
                     didUpdateURL:(const GURL&)URL {
  [_mediator loadURL:URL];
}

@end
