// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_scene_agent.h"

#import <MaterialComponents/MaterialSnackbar.h>
#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "components/enterprise/idle/metrics.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_ui_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/constants.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_coordinator.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_coordinator_delegate.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_launch_screen_view_controller.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_utils.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
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

  // Flag indicating whether this dialog is allowed to display the snackbar.
  // This is used to show the snackbar on the same scene that shows the timeout
  // confirmation dialog.
  BOOL _pendingDisplayingSnackbar;

  // Coordinator for the idle timeout confirmation dialog.
  IdleTimeoutConfirmationCoordinator* _idleTimeoutConfirmationCoordinator;

  // An extended launch screen that shows on start-up or re-foreground. The
  // windows shows on top of the browser to block the user from navigating when
  // data is cleared, and to hide the triggered UI changes when tabs are closed
  // or the user is signed out.
  UIWindow* _launchScreenWindow;
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
  // Show or remove the launch screen based on the scene activation level.
  [self handleExtendedLaunchScreenWindowForSceneActivationLevel:level];
  // Monitor the scene activation level to consider showing the idle timeout
  // snackbar when the scene becomes active and in the foreground. This is
  // needed because the scene state might not be foregrounded yet when
  // `onIdleTimeoutActionsCompleted` is called on foreground.
  [self maybeShowPostActionSnackbar];
}

#pragma mark - IdleServiceObserving

- (void)onIdleTimeoutInForeground {
  [self maybeShowIdleTimeoutConfirmationDialog];
}

- (void)onIdleTimeoutOnStartup {
  CHECK(_idleService->IsIdleTimeoutPolicySet());
  // Any window can display the snackbar after actions run on startup or
  // reforeground. The differentiating factor in this case will be which scene
  // enters foreground first.
  _pendingDisplayingSnackbar = YES;
  [self showExtendedLaunchScreenWindow];
}

- (void)onIdleTimeoutActionsCompleted {
  [self maybeDismissExtendedLaunchScreenWindowIfDisplayed];
  [self maybeShowPostActionSnackbar];
}

- (void)onApplicationWillEnterBackground {
  CHECK(_idleService->IsIdleTimeoutPolicySet());
  [self stopIdleTimeoutConfirmationCoordinator];
  // When the app is moving to the background -> Show the launch screen. This
  // needs to be done now instead of when we are sure the app will be idle on
  // foreground. The reason is when the app is reforegrounded, there is a second
  // when the initial snapshot of the UI is shown. The screen will be removed on
  // reforeground if it is not actually needed (i.e. the browser has not timed
  // out).
  [self showExtendedLaunchScreenWindow];
}

#pragma mark - IdleTimeoutConfirmationCoordinatorDelegate

- (void)stopPresentingAndRunActionsAfterwards:(BOOL)doRunActions {
  _idleService->OnIdleTimeoutDialogPresented();
  [self stopIdleTimeoutConfirmationCoordinator];

  if (doRunActions) {
    enterprise_idle::metrics::RecordIdleTimeoutDialogEvent(
        enterprise_idle::metrics::IdleTimeoutDialogEvent::kDialogExpired);
    _pendingDisplayingSnackbar = YES;
    _idleService->RunActions();
  } else {
    enterprise_idle::metrics::RecordIdleTimeoutDialogEvent(
        enterprise_idle::metrics::IdleTimeoutDialogEvent::
            kDialogDismissedByUser);
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

- (PrefService*)prefService {
  return _mainBrowser->GetProfile()->GetPrefs();
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
  if ([self isLaunchScreenDisplayed]) {
    // The snackbar will show after the launch screen is hidden.
    return;
  }
  if (![self isUIAvailableToShowSnackbar]) {
    return;
  }

  if (!_idleService->ShouldIdleTimeoutSnackbarBePresented()) {
    // Returns if the snackbar is no longer needed and has been handled by
    // another agent which has set the flag to false.  This flag check is
    // important for the case when the actions run on startup/reforeground where
    // `onIdleTimeoutActionsCompleted` may be called before
    // `transitionedToActivationLevel` to foreground.
    return;
  }

  // It is important to get the last actions from the service because the window
  // showing the snackbar might have been opened after timeout happened. This
  // can be the case in the following scenario: Foreground 1 window -> wait till
  // dialog shows -> open another window -> Now close the window that initially
  // showed the dialog.
  std::optional<int> messageId =
      enterprise_idle::GetIdleTimeoutActionsSnackbarMessageId(
          _idleService->GetLastActionSet());
  CHECK(messageId) << "There is no snackbar message for the set of actions";
  NSString* messageText = l10n_util::GetNSString(*messageId);

  // Delay showing the snackbar message when voice over is on because other
  // elements with higher accessibility priority will cut off reading the
  // snackbar message. For example, when tabs are closed on idle timeout, the
  // snackbar message is cut off when the screen reader reads out the text on
  // the empty tab grid that got displayed, so we need to wait.
  if (UIAccessibilityIsVoiceOverRunning()) {
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf showSnackbar:messageText];
        }),
        base::Seconds(2));
  } else {
    [self showSnackbar:messageText];
  }
  _idleService->OnIdleTimeoutSnackbarPresented();
}

- (void)showSnackbar:(NSString*)messageText {
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageText);
  message.duration = kIdleTimeoutSnackbarDuration;
  message.accessibilityLabel = messageText;
  [_snackbarHandler showSnackbarMessage:message];
}

// Returns whether the scene and app states allow for the idle timeout
// confirmation dialog to be shown if it is needed.
- (BOOL)isUIAvailableToShowDialog {
  if (self.sceneState.appState.initStage < AppInitStage::kFinal) {
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
  [self closeMediaPresentationsIfFullScreenMode];
  _idleTimeoutConfirmationCoordinator =
      [[IdleTimeoutConfirmationCoordinator alloc]
          initWithBaseViewController:[_sceneUIProvider activeViewController]
                             browser:_mainBrowser];
  _idleTimeoutConfirmationCoordinator.delegate = self;
  _idleTimeoutConfirmationCoordinator.triggerTime =
      _idleService->GetIdleTriggerTime();
  [_idleTimeoutConfirmationCoordinator start];
  enterprise_idle::metrics::RecordIdleTimeoutDialogEvent(
      enterprise_idle::metrics::IdleTimeoutDialogEvent::kDialogShown);
}

// Dismisses the idle timeout confirmation dialog.
- (void)stopIdleTimeoutConfirmationCoordinator {
  _UIBlocker.reset();
  if (_idleTimeoutConfirmationCoordinator) {
    [_idleTimeoutConfirmationCoordinator stop];
    _idleTimeoutConfirmationCoordinator = nil;
  }
}

- (void)showExtendedLaunchScreenWindow {
  // Return if the window is already displayed. This happens when the app has
  // been backgrounded with the policy set then re-foregrounded, in which case
  // the app has the launch screen window on re-foregrounded.
  if ([self isLaunchScreenDisplayed]) {
    return;
  }

  IdleTimeoutLaunchScreenViewController* _launchScreenController =
      [[IdleTimeoutLaunchScreenViewController alloc] init];
  _launchScreenWindow =
      [[UIWindow alloc] initWithWindowScene:self.sceneState.scene];
  // The blocker is above everything, including the alerts, but below the status
  // bar.
  _launchScreenWindow.windowLevel = UIWindowLevelStatusBar - 1;
  _launchScreenWindow.rootViewController = _launchScreenController;
  [_launchScreenWindow makeKeyAndVisible];
  [self scheduleLaunchScreenDismissal];
}

- (void)maybeDismissExtendedLaunchScreenWindowIfDisplayed {
  if (![self isLaunchScreenDisplayed]) {
    // Nothing needs to be done here, so we can return.
    return;
  }

  enterprise_idle::metrics::RecordIdleTimeoutLaunchScreenEvent(
      enterprise_idle::metrics::IdleTimeoutLaunchScreenEvent::
          kLaunchScreenDismissedAfterActionCompletion);

  if (!_idleService->GetLastActionSet().close) {
    // Dismiss right away if tabs will not be closing, which is often delayed.
    [self dismissExtendedLaunchScreenWindowIfDisplayed];
    return;
  }

  // Remove after 1 more second to give the UI enough time to update behind the
  // screen after actions have run. If the screen is dimssed right away, the
  // tabs will be seen closing.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf dismissExtendedLaunchScreenWindowIfDisplayed];
        [weakSelf maybeShowPostActionSnackbar];
      }),
      base::Seconds(1));
}

- (void)dismissExtendedLaunchScreenWindowIfDisplayed {
  if (![self isLaunchScreenDisplayed]) {
    // Nothing needs to be done here, so we can return.
    return;
  }

  _launchScreenWindow = nil;
  [self.sceneState.window makeKeyAndVisible];
}

- (BOOL)isLaunchScreenDisplayed {
  return _launchScreenWindow != nil;
}

- (void)handleExtendedLaunchScreenWindowForSceneActivationLevel:
    (SceneActivationLevel)level {
  // When the backgrounded app is moving back to the foreground -> dismiss the
  // launch screen if the browser did not timeout. Otherwise keep it, and it
  // will be dismissed later when the service completes running the idle
  // actions, or when it has exceeded 5 seconds.
  bool isReforegroundedWithLaunchScreen =
      (level == SceneActivationLevelForegroundInactive) && _launchScreenWindow;
  if (isReforegroundedWithLaunchScreen) {
    if (!_idleService->IsIdleAfterPreviouslyBeingActive()) {
      [self dismissExtendedLaunchScreenWindowIfDisplayed];
    } else {
      [self scheduleLaunchScreenDismissal];
    }
  }
}

- (void)scheduleLaunchScreenDismissal {
  // Set a deadline for the dismissal of the launch screen so the user never
  // waits indefinitely.
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        enterprise_idle::metrics::RecordIdleTimeoutLaunchScreenEvent(
            enterprise_idle::metrics::IdleTimeoutLaunchScreenEvent::
                kLaunchScreenExpired);
        [weakSelf dismissExtendedLaunchScreenWindowIfDisplayed];
      }),
      base::Seconds(5));
}

// Closes the media presentations to avoid having the fullscreen video on top of
// the dialog so the user does not miss the dialog if they are watching a video.
- (void)closeMediaPresentationsIfFullScreenMode {
  Browser* currentBrowser =
      self.sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(currentBrowser);
  web::WebState* activeWebState =
      currentBrowser->GetWebStateList()->GetActiveWebState();
  if (activeWebState) {
    activeWebState->CloseMediaPresentations();
  }
}

@end
