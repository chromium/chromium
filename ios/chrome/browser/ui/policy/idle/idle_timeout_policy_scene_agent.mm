// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/idle/idle_timeout_policy_scene_agent.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/policy/idle/idle_timeout_confirmation_coordinator.h"
#import "ios/chrome/browser/ui/policy/idle/idle_timeout_confirmation_coordinator_delegate.h"
#import "ios/chrome/browser/ui/policy/idle/idle_timeout_policy_utils.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface IdleTimeoutPolicySceneAgent () <
    AppStateObserver,
    IdleServiceObserving,
    IdleTimeoutConfirmationCoordinatorDelegate>
@end

@implementation IdleTimeoutPolicySceneAgent {
  // Scoped UI blocker that blocks the other scenes/windows if the dialog is
  // shown on this scene.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;

  // Observes changes in the idle state.
  std::unique_ptr<IdleServiceObserverBridge> _idleServiceObserverBridge;

  // Browser of the main interface of the scene.
  raw_ptr<Browser> _mainBrowser;

  // SceneUIProvider that provides the scene UI objects.
  id<SceneUIProvider> _sceneUIProvider;

  // Handler for application commands.
  __weak id<ApplicationCommands> _applicationHandler;

  // Handler for application commands.
  __weak id<SnackbarCommands> _snackbarHandler;

  // Service handling IdleTimeout and IdleTimeoutActions policies.
  // IdleTimeoutPolicySceneAgents observe this service.
  raw_ptr<enterprise_idle::IdleService> _idleService;

  // The time `onIdleTimeoutInForeground` is triggered. Used to determine the
  // start of the countdown displayed. Usually the countdown is 30s, but might
  // need to be adjusted if the dialog was already started on a different scene.
  base::Time _idleTriggerTime;

  // Flag indicating whether this dialog is allowed to display the snackbar.
  // This is used to show the snackbar on the same scene that shows the timeout
  // confirmation dialog.
  BOOL _pendingDisplayingSnackbar;

  // Coordinator for the idle timeout confirmation dialog.
  IdleTimeoutConfirmationCoordinator* _idleTimeoutConfirmationCoordinator;

  // The actions that will run on idle timeout.
  enterprise_idle::ActionSet _actions;
}

- (instancetype)
       initWithSceneUIProvider:(id<SceneUIProvider>)sceneUIProvider
    applicationCommandsHandler:(id<ApplicationCommands>)applicationHandler
       snackbarCommandsHandler:(id<SnackbarCommands>)snackbarHandler
                   idleService:(enterprise_idle::IdleService*)idleService
                   mainBrowser:(Browser*)mainBrowser {
  self = [super init];
  if (self) {
    _sceneUIProvider = sceneUIProvider;
    _applicationHandler = applicationHandler;
    _snackbarHandler = snackbarHandler;
    _mainBrowser = mainBrowser;
    _idleService = idleService;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  [self.sceneState.appState addObserver:self];
}

#pragma mark - SceneStateObserver

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Tear down objects tied to the scene state before it is deleted.
  [self tearDownObservers];
  _mainBrowser = nullptr;
  [self stopIdleTimeoutConfirmationCoordinator];
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  // Setup objects that need the browser UI objects before being set.
  [self setupObserver];
}

- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState {
  // Called to check if the dialog needs to be shown after a UI blocker has been
  // released. This is the case when one scene is closed while showing the
  // dialog, so any other open scene should take over showing the countdown.
  [self maybeShowIdleTimeoutConfirmationDialog];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Monitor the scene activation level to consider showing the idle timeout
  // snackbar when the scene becomes active and in the foreground. This is
  // needed because the scene state might not be foregrounded yet when
  // `onIdleTimeoutActionsCompleted` is called on foreground.
  [self maybeShowPostActionSnackbar];
}

#pragma mark - IdleServiceObserving

- (void)onIdleTimeoutInForeground {
  _idleTriggerTime = base::Time::Now();
  _actions =
      enterprise_idle::GetActionSet([self prefService], [self authService]);
  [self maybeShowIdleTimeoutConfirmationDialog];
}

- (void)onIdleTimeoutOnStartup {
  // Any window can display the snackbar after actions run on startup or
  // reforeground. The differentiating factor in this case will be which scene
  // enters foreground first.
  _pendingDisplayingSnackbar = YES;
  _actions =
      enterprise_idle::GetActionSet([self prefService], [self authService]);
  // TODO(b/301676922): Show loading window if data will be cleared.
}

- (void)onIdleTimeoutActionsCompleted {
  [self maybeShowPostActionSnackbar];
}

- (void)onApplicationWillEnterBackground {
  [self stopIdleTimeoutConfirmationCoordinator];
}

#pragma mark - IdleTimeoutConfirmationCoordinatorDelegate

- (void)stopPresentingAndRunActionsAfterwards:(BOOL)doRunActions {
  _idleService->OnIdleTimeoutDialogPresented();
  [self stopIdleTimeoutConfirmationCoordinator];
  if (doRunActions) {
    _pendingDisplayingSnackbar = YES;
    _idleService->RunActions();
  } else {
    _pendingDisplayingSnackbar = NO;
  }
}

#pragma mark - Private

- (void)setupObserver {
  // Set observer for service status changes.
  _idleServiceObserverBridge =
      std::make_unique<IdleServiceObserverBridge>(_idleService, self);
}

- (void)tearDownObservers {
  _idleServiceObserverBridge.reset();
  [self.sceneState.appState removeObserver:self];
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      _mainBrowser->GetBrowserState());
}

- (PrefService*)prefService {
  return _mainBrowser->GetBrowserState()->GetPrefs();
}

// Returns whether the scene and app states allow for the idle timeout snackbar
// to show if idle actions have run.
- (BOOL)isUIAvailableToShowSnackbar {
  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    // Return NO when the scene isn't visible, active, and in the foreground.
    return NO;
  }

  // Return whether this is the agent that should display the
  // snackbar. If there was a timeout dialog, only one agent would have this
  // state set.
  return _pendingDisplayingSnackbar;
}

// Shows the actions ran snackbar using the snackbar command.
- (void)maybeShowPostActionSnackbar {
  if (![self isUIAvailableToShowSnackbar]) {
    return;
  }

  if (!_idleService->ShouldIdleTimeoutSnackbarBePresented()) {
    // Returns if the snackbar is no longer needed and has been handled by
    // another agent which has set the flag to false.  This flag check is
    // important for the case when the actions run on startup/refreground where
    // `onIdleTimeoutActionsCompleted` may be called before
    // `transitionedToActivationLevel` to foreground.
    return;
  }

  std::optional<int> messageId =
      enterprise_idle::GetIdleTimeoutActionsSnackbarMessageId(_actions);
  CHECK(messageId) << "There is no snackbar message for the set of actions";
  NSString* messageText = l10n_util::GetNSString(*messageId);
  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:messageText];
  [_snackbarHandler showSnackbarMessage:message];

  _idleService->OnIdleTimeoutSnackbarPresented();
}

// Returns whether the scene and app states allow for the idle timeout
// confirmation dialog to be shown if it is needed.
- (BOOL)isUIAvailableToShowDialog {
  if (self.sceneState.appState.initStage < InitStageFinal) {
    // Return NO when the app isn't yet fully initialized.
    return NO;
  }

  if (self.sceneState.activationLevel < SceneActivationLevelForegroundActive) {
    // Return NO when the scene isn't visible, active, and in the foreground.
    return NO;
  }

  if (_UIBlocker != nil) {
    // Return NO if scene is already showing the dialog as there is no need to
    // reshow it with a disturbing transition.
    // This is for the case when the scene displaying the dialog is backgrounded
    // while there is another scene with a modal overlay opened. Going back to
    // the window showing the modal overlay should continue showing the same
    // dialog.
    return NO;
  }

  // Return YES if the scene is not blocked by a modal overlay.
  return !self.sceneState.presentingModalOverlay;
}

// Shows the notification dialog if these two conditions are satisfied:
// 1. the UI is available
// 2. it was never shown or if a scene displaying the dialog
// was closed and anoher foregrouded window remained open.
- (void)maybeShowIdleTimeoutConfirmationDialog {
  // Initially set the pending snackbar flag to false in case it was set on
  // startup but actions failed to complete.
  _pendingDisplayingSnackbar = NO;
  if (![self isUIAvailableToShowDialog]) {
    return;
  }

  if (!_idleService->ShouldIdleTimeoutDialogBePresented()) {
    // Skip the dialog if it has already been displayed until expiry, in which
    // case it will not be needed.
    return;
  }

  // Set the pending snackbar flag for the agent that will show the dialog then
  // show then dismiss any modals and display the dialog.
  _pendingDisplayingSnackbar = YES;
  _UIBlocker = std::make_unique<ScopedUIBlocker>(self.sceneState);
  __weak __typeof(self) weakSelf = self;
  [_applicationHandler dismissModalDialogsWithCompletion:^{
    [weakSelf showIdleTimeoutConfirmation];
  }];
}

// Shows the notification dialog for the account on the `viewController`
- (void)showIdleTimeoutConfirmation {
  _idleTimeoutConfirmationCoordinator =
      [[IdleTimeoutConfirmationCoordinator alloc]
          initWithBaseViewController:[_sceneUIProvider activeViewController]
                             browser:_mainBrowser];
  _idleTimeoutConfirmationCoordinator.delegate = self;
  _idleTimeoutConfirmationCoordinator.triggerTime = _idleTriggerTime;
  [_idleTimeoutConfirmationCoordinator start];
}

// Dismisses the idle timeout confirmation dialog.
- (void)stopIdleTimeoutConfirmationCoordinator {
  _UIBlocker.reset();
  if (_idleTimeoutConfirmationCoordinator) {
    [_idleTimeoutConfirmationCoordinator stop];
    _idleTimeoutConfirmationCoordinator = nil;
  }
}

@end
