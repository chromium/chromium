// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_load_url.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

namespace {

// Records a SigninFullscreenPromoEvents UMA histogram.
void RecordIfNeededSigninFullscreenPromoEvent(
    SigninFullscreenPromoEvents event,
    signin_metrics::AccessPoint accessPoint) {
  if (accessPoint != signin_metrics::AccessPoint::kFullscreenSigninPromo) {
    return;
  }
  base::UmaHistogramEnumeration("IOS.SignInpromo.Fullscreen.PromoEvents",
                                event);
}

}  // namespace

@interface SceneCoordinator () <AccountMenuCoordinatorDelegate,
                                PolicyWatcherBrowserAgentObserving,
                                SafariDataImportMainCoordinatorDelegate>

// The SceneState for this scene.
@property(nonatomic, readonly) SceneState* sceneState;

// The profile for this scene.
@property(nonatomic, readonly) ProfileIOS* profile;

// The Browser for the current interface.
@property(nonatomic, readonly) Browser* currentBrowser;

@end

@implementation SceneCoordinator {
  id<SceneCommands> _sceneCommandsEndpoint;
  base::WeakPtr<Browser> _inactiveBrowser;
  base::WeakPtr<Browser> _regularBrowser;
  // Coordinator for the Tab Grid
  TabGridCoordinator* _tabGridCoordinator;
  // Coordinator for the account menu.
  AccountMenuCoordinator* _accountMenuCoordinator;
  // Coordinator for the sign-in flow.
  SigninCoordinator* _signinCoordinator;
  // Coordinator for the Safari Data Import flow.
  SafariDataImportMainCoordinator* _safariDataImportCoordinator;
  // Observer for PolicyWatcherBrowserAgent.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
  std::unique_ptr<
      base::ScopedObservation<PolicyWatcherBrowserAgent,
                              PolicyWatcherBrowserAgentObserverBridge>>
      _policyWatcherObserver;
}

- (instancetype)initWithSceneCommandsEndpoint:
                    (id<SceneCommands>)sceneCommandsEndpoint
                               regularBrowser:(Browser*)regularBrowser
                              inactiveBrowser:(Browser*)inactiveBrowser
                             incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super init])) {
    _sceneCommandsEndpoint = sceneCommandsEndpoint;
    _regularBrowser = regularBrowser->AsWeakPtr();
    _inactiveBrowser = inactiveBrowser->AsWeakPtr();
    _incognitoBrowser = incognitoBrowser;
  }
  return self;
}

- (void)start {
  _policyWatcherObserverBridge =
      std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);
  _policyWatcherObserver = std::make_unique<base::ScopedObservation<
      PolicyWatcherBrowserAgent, PolicyWatcherBrowserAgentObserverBridge>>(
      _policyWatcherObserverBridge.get());
  PolicyWatcherBrowserAgent* policyWatcherAgent =
      PolicyWatcherBrowserAgent::FromBrowser(_regularBrowser.get());
  _policyWatcherObserver->Observe(policyWatcherAgent);

  _tabGridCoordinator = [[TabGridCoordinator alloc]
      initWithSceneCommandsEndpoint:_sceneCommandsEndpoint
                     regularBrowser:_regularBrowser.get()
                    inactiveBrowser:_inactiveBrowser.get()
                   incognitoBrowser:_incognitoBrowser];
  _tabGridCoordinator.delegate = self.delegate;
  [_tabGridCoordinator start];
}

- (void)stop {
  _policyWatcherObserver.reset();
  _policyWatcherObserverBridge.reset();
  [self stopAccountMenu];
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  [self stopSafariDataImportCoordinator];
  [_tabGridCoordinator stop];
}

#pragma mark - Public

- (BOOL)isTabGridActive {
  return _tabGridCoordinator.isTabGridActive;
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  [_tabGridCoordinator stopChildCoordinatorsWithCompletion:completion];
}

- (void)showTabGridPage:(TabGridPage)page {
  [_tabGridCoordinator showTabGridPage:page];
}

- (void)showTabViewController:(UIViewController*)viewController
                    incognito:(BOOL)incognito
                   completion:(ProceduralBlock)completion {
  [_tabGridCoordinator showTabViewController:viewController
                                   incognito:incognito
                                  completion:completion];
}

- (void)setActiveMode:(TabGridMode)mode {
  [_tabGridCoordinator setActiveMode:mode];
}

- (BOOL)isSigninInProgress {
  return _signinCoordinator != nil;
}

- (void)showAccountMenuFromWebWithURL:(const GURL&)URL {
  if (_accountMenuCoordinator) {
    return;
  }
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:self.activeViewController
                         browser:_regularBrowser.get()
                      anchorView:nil
                     accessPoint:AccountMenuAccessPoint::kWeb
                             URL:URL];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  if (!baseViewController) {
    baseViewController = self.activeViewController;
  }
  if (![self canPresentSigninCoordinatorOrCompletion:command.completion
                                  baseViewController:baseViewController
                                         accessPoint:command.accessPoint]) {
    return;
  }
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  _signinCoordinator =
      [SigninCoordinator signinCoordinatorWithCommand:command
                                              browser:_regularBrowser.get()
                                   baseViewController:baseViewController];
  [self startSigninCoordinatorWithCompletion:command.completion];
}

- (void)showFullscreenSigninPromoWithCompletion:
    (SigninCoordinatorCompletionCallback)dismissalCompletion {
  DCHECK(!_signinCoordinator)
      << "_signinCoordinator: "
      << base::SysNSStringToUTF8([_signinCoordinator description]);
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  _signinCoordinator = [SigninCoordinator
      fullscreenSigninPromoCoordinatorWithBaseViewController:
          self.activeViewController
                                                     browser:_regularBrowser
                                                                 .get()
                                                contextStyle:
                                                    SigninContextStyle::kDefault
                           changeProfileContinuationProvider:
                               DoNothingContinuationProvider()];
  [self startSigninCoordinatorWithCompletion:dismissalCompletion];
}

- (void)showWebSigninPromoFromViewController:(UIViewController*)viewController
                                         URL:(const GURL&)URL {
  if (!signin::ShouldPresentWebSignin(_regularBrowser->GetProfile())) {
    return;
  }
  id<BrowserCoordinatorCommands> browserCoordinatorHandler = HandlerForProtocol(
      self.currentBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  void (^prepareChangeProfile)() = ^() {
    [browserCoordinatorHandler closeCurrentTab];
  };
  ChangeProfileContinuationProvider provider =
      base::BindRepeating(&CreateChangeProfileOpensURLContinuation, URL);
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  base::WeakPtr<Browser> regularBrowser = _regularBrowser;
  _signinCoordinator = [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:viewController
                                                      browser:regularBrowser
                                                                  .get()
                                                 contextStyle:
                                                     SigninContextStyle::
                                                         kDefault
                                                  accessPoint:signin_metrics::
                                                                  AccessPoint::
                                                                      kWebSignin
                                         prepareChangeProfile:
                                             prepareChangeProfile
                                         continuationProvider:provider];
  if (!_signinCoordinator) {
    return;
  }
  // Copy the URL so it can be safely captured in the block.
  GURL copiedURL = URL;
  [self startSigninCoordinatorWithCompletion:^(SigninCoordinator* coordinator,
                                               SigninCoordinatorResult result,
                                               id<SystemIdentity> identity) {
    if (result == SigninCoordinatorResultSuccess && regularBrowser) {
      UrlLoadingBrowserAgent::FromBrowser(regularBrowser.get())
          ->Load(UrlLoadParams::InCurrentTab(copiedURL));
    }
  }];
}

- (void)stopSigninCoordinatorWithCompletionAnimated:(BOOL)animated {
  // We retain the coordinator until the end of the completion, while ensuring
  // that when the completion requests `self` to stop the signin coordinator,
  // `stop` is not called a second time.
  SigninCoordinator* signinCoordinator = _signinCoordinator;
  if (!signinCoordinator) {
    return;
  }
  _signinCoordinator = nil;

  [signinCoordinator stopAnimated:animated];
  SigninCoordinatorCompletionCallback signinCompletion =
      signinCoordinator.signinCompletion;
  signinCoordinator.signinCompletion = nil;
  CHECK(signinCompletion, base::NotFatalUntil::M142);
  // The `signinCoordinator` must be nil here, because `_signinCoordinator`
  // was set to `nil` above.
  signinCompletion(nil, SigninCoordinatorResultInterrupted, nil);
}

- (void)displaySafariDataImportFromEntryPoint:
            (SafariDataImportEntryPoint)entryPoint
                                withUIHandler:
                                    (id<SafariDataImportUIHandler>)UIHandler
                           baseViewController:
                               (UIViewController*)baseViewController {
  if (_safariDataImportCoordinator) {
    return;
  }
  CHECK(ShouldShowSafariDataImportEntryPoint(
      self.currentBrowser->GetProfile()->GetPrefs()));

  _safariDataImportCoordinator = [[SafariDataImportMainCoordinator alloc]
          initFromEntryPoint:entryPoint
      withBaseViewController:baseViewController
                     browser:self.currentBrowser];
  _safariDataImportCoordinator.delegate = self;
  _safariDataImportCoordinator.UIHandler = UIHandler;
  [_safariDataImportCoordinator start];
}

- (void)stopSafariDataImportCoordinator {
  [_safariDataImportCoordinator stop];
  _safariDataImportCoordinator = nil;
}

#pragma mark - Properties

- (void)setDelegate:(id<TabGridCoordinatorDelegate>)delegate {
  _delegate = delegate;
  _tabGridCoordinator.delegate = delegate;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
  _tabGridCoordinator.incognitoBrowser = incognitoBrowser;
}

- (UIViewController*)activeViewController {
  return _tabGridCoordinator.activeViewController;
}

- (SceneState*)sceneState {
  return _regularBrowser->GetSceneState();
}

- (ProfileIOS*)profile {
  return self.sceneState.profileState.profile;
}

- (Browser*)currentBrowser {
  return self.sceneState.browserProviderInterface.currentBrowserProvider
      .browser;
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  if (_signinCoordinator) {
    [self stopSigninCoordinatorWithCompletionAnimated:YES];
    base::UmaHistogramBoolean(
        "Enterprise.BrowserSigninIOS.SignInInterruptedByPolicy", true);
    policyWatcher->SignInUIDismissed();
  }
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  CHECK_EQ(_accountMenuCoordinator, coordinator);
  [self stopAccountMenu];
}

#pragma mark - SafariDataImportMainCoordinatorDelegate

- (void)safariImportWorkflowDidEndForCoordinator:
    (SafariDataImportMainCoordinator*)coordinator {
  CHECK_EQ(coordinator, _safariDataImportCoordinator);
  [self stopSafariDataImportCoordinator];
}

#pragma mark - Private

// Stops the account menu coordinator.
- (void)stopAccountMenu {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

// Returns `YES` if a signin coordinator can be opened by the scene coordinator.
// Otherwise, execute the completion with `SigninCoordinatorUINotAvailable`.
// Fails if another signin coordinator is already opened.
- (BOOL)canPresentSigninCoordinatorOrCompletion:
            (SigninCoordinatorCompletionCallback)completion
                             baseViewController:
                                 (UIViewController*)baseViewController
                                    accessPoint:(signin_metrics::AccessPoint)
                                                    accessPoint {
  if (_signinCoordinator) {
    // As of M121, the CHECK bellow is known to fire in various cases. The goal
    // of the histograms below is to detect the number of incorrect cases and
    // for which of the access points they are triggered.
    base::UmaHistogramEnumeration(
        "Signin.ShowSigninCoordinatorWhenAlreadyPresent.NewAccessPoint",
        accessPoint);
    base::UmaHistogramEnumeration(
        "Signin.ShowSigninCoordinatorWhenAlreadyPresent.OldAccessPoint",
        _signinCoordinator.accessPoint);
    // The goal of this histogram is to understand if the issue is related to
    // a double tap (duration less than 1s), or if `self.signinCoordinator`
    // is not visible anymore on the screen (duration more than 1s).
    const base::TimeDelta duration =
        base::TimeTicks::Now() - _signinCoordinator.creationTimeTicks;
    UmaHistogramTimes("Signin.ShowSigninCoordinatorWhenAlreadyPresent."
                      "DurationBetweenTwoSigninCoordinatorCreation",
                      duration);
  }
  // TODO(crbug.com/40071586): Change this to a CHECK once this invariant is
  // correct.
  DCHECK(!_signinCoordinator)
      << "self.signinCoordinator: "
      << base::SysNSStringToUTF8([_signinCoordinator description]);
  return YES;
}

// Starts the sign-in coordinator and sets its completion.
- (void)startSigninCoordinatorWithCompletion:
    (SigninCoordinatorCompletionCallback)completion {
  DCHECK(_signinCoordinator);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  AuthenticationService::ServiceStatus statusService =
      authenticationService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy: {
      if (completion) {
        // The coordinator argument is `nil` because this completion has never
        // been assigned to a signinCoordinator’s `signinCompletion`. It works
        // because the part that check the coordinator value is in the
        // `signinCompletedWithCoordinator:...` below, and so not integrated in
        // the completion function yet.
        completion(nil, SigninCoordinatorResultDisabled, nil);
      }
      [self stopSigninCoordinatorAnimated:NO];
      id<PolicyChangeCommands> handler =
          HandlerForProtocol(_signinCoordinator.browser->GetCommandDispatcher(),
                             PolicyChangeCommands);
      [handler showForceSignedOutPrompt];
      RecordIfNeededSigninFullscreenPromoEvent(
          SigninFullscreenPromoEvents::kPromoCanceledByPolicy,
          _signinCoordinator.accessPoint);
      return;
    }
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed: {
      break;
    }
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser: {
      DUMP_WILL_BE_NOTREACHED()
          << "Status service: " << static_cast<int>(statusService);
      break;
    }
  }

  DCHECK(_signinCoordinator);

  if (self.sceneState.isUIBlocked) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    if (completion) {
      // The coordinator argument is `nil` because this completion has never
      // been assigned to a signinCoordinator’s `signinCompletion`. It works
      // because the part that check the coordinator value is in the
      // `signinCompletedWithCoordinator:...` below, and so not integrated in
      // the completion function yet.
      completion(nil, SigninCoordinatorResultInterrupted, nil);
    }
    _signinCoordinator = nil;
    RecordIfNeededSigninFullscreenPromoEvent(
        SigninFullscreenPromoEvents::kPromoCanceledByUIBlocked,
        _signinCoordinator.accessPoint);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinCompletedWithCoordinator:coordinator
                                          result:result
                                        identity:identity
                                      completion:completion];
      };

  // Log that the fullscreen sign-in promo UI has started.
  RecordIfNeededSigninFullscreenPromoEvent(
      SigninFullscreenPromoEvents::kPromoUIStarted,
      _signinCoordinator.accessPoint);

  [_signinCoordinator start];
}

// Stops the sign-in coordinator without running the completion.
- (void)stopSigninCoordinatorAnimated:(BOOL)animated {
  // This ensure that when the SceneCoordinator receives the `signinFinished`
  // command, it does not detect the SigninCoordinator as still presented.
  SigninCoordinator* signinCoordinator = _signinCoordinator;
  _signinCoordinator = nil;
  [signinCoordinator stopAnimated:animated];
}

// Called when the sign-in coordinator finishes.
- (void)signinCompletedWithCoordinator:(SigninCoordinator*)coordinator
                                result:(SigninCoordinatorResult)result
                              identity:(id<SystemIdentity>)identity
                            completion:(SigninCoordinatorCompletionCallback)
                                           completion {
  CHECK_EQ(coordinator, _signinCoordinator, base::NotFatalUntil::M151);

  if (completion) {
    completion(coordinator, result, identity);
  }
  [self stopSigninCoordinatorAnimated:YES];
}

@end
