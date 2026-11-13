// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/signin_modal_host.h"

#import <memory>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_load_url.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/scene/coordinator/signin_modal_host_delegate.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

namespace {

// Records a `SigninFullscreenPromoEvents` UMA histogram.
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

@interface SigninModalHost () <PolicyWatcherBrowserAgentObserving>
@end

@implementation SigninModalHost {
  raw_ptr<Browser> _regularBrowser;
  raw_ptr<Browser> _inactiveBrowser;
  raw_ptr<Browser> _incognitoBrowser;
  __weak UIViewController* _baseViewController;
  // Coordinator for the sign-in flow.
  SigninCoordinator* _signinCoordinator;
  // Observer for `PolicyWatcherBrowserAgent`, to stop the sign-in flow when
  // sign-in gets disabled by policy.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
  std::unique_ptr<
      base::ScopedObservation<PolicyWatcherBrowserAgent,
                              PolicyWatcherBrowserAgentObserverBridge>>
      _policyWatcherObserver;
}

- (instancetype)initWithRegularBrowser:(Browser*)regularBrowser
                      incognitoBrowser:(Browser*)incognitoBrowser {
  self = [super init];
  if (self) {
    _regularBrowser = regularBrowser;
    _incognitoBrowser = incognitoBrowser;
  }
  return self;
}

#pragma mark - Public

- (void)startHostingCommandProtocols {
  CHECK(_regularBrowser);
  _inactiveBrowser = _regularBrowser->GetInactiveBrowser();
  [self startDispatchingForBrowser:_regularBrowser.get()];
  [self startDispatchingForBrowser:_inactiveBrowser.get()];
  [self startDispatchingForBrowser:_incognitoBrowser.get()];
  [self startObservingPolicyWatcher];
}

- (void)connectIncognitoBrowser:(Browser*)incognitoBrowser {
  CHECK(incognitoBrowser);
  CHECK(!_incognitoBrowser)
      << "-disconnectIncognitoBrowser must be called first";
  _incognitoBrowser = incognitoBrowser;
  [self startDispatchingForBrowser:_incognitoBrowser.get()];
}

- (void)disconnectIncognitoBrowser {
  CHECK(_incognitoBrowser) << "No incognito browser is connected";
  [self stopDispatchingForBrowser:_incognitoBrowser.get()];
  _incognitoBrowser = nullptr;
}

- (void)setBaseViewControllerForModals:(UIViewController*)baseViewController {
  _baseViewController = baseViewController;
}

- (void)clearPresentedStateAnimated:(BOOL)animated {
  [self stopSigninCoordinatorWithCompletionAnimated:animated];
}

- (void)stopHostingCommandProtocols {
  _policyWatcherObserver.reset();
  _policyWatcherObserverBridge.reset();
  // Unregisters before dismissing, so that the commands sent by the dismissal
  // completions don't reach this host during teardown.
  [self stopDispatchingForBrowser:_regularBrowser.get()];
  [self stopDispatchingForBrowser:_inactiveBrowser.get()];
  [self stopDispatchingForBrowser:_incognitoBrowser.get()];
  [self clearPresentedStateAnimated:NO];

  _regularBrowser = nullptr;
  _inactiveBrowser = nullptr;
  _incognitoBrowser = nullptr;
}

#pragma mark - SceneSignInCommands

- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  if (!baseViewController) {
    baseViewController = [self activeViewController];
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

- (void)showWebSigninPromoFromViewController:(UIViewController*)viewController
                                         URL:(const GURL&)URL {
  // Do not display the web sign-in promo if there is any UI on the screen.
  if (viewController.presentedViewController ||
      ![self.delegate
          isTabAvailableToPresentViewControllerForSigninModalHost:self]) {
    return;
  }
  if (!signin::ShouldPresentWebSignin(_regularBrowser->GetProfile())) {
    return;
  }
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  _signinCoordinator =
      [self webSigninCoordinatorWithBaseViewController:viewController URL:URL];
  base::WeakPtr<Browser> regularBrowser = _regularBrowser->AsWeakPtr();
  // Copy the URL so it can be safely captured in the block.
  GURL copiedURL = URL;
  [self startSigninCoordinatorWithCompletion:^(
            SigninCoordinator* coordinator,
            SigninCoordinatorResultOrIdentity result) {
    if (result.has_value() && regularBrowser) {
      UrlLoadingBrowserAgent::FromBrowser(regularBrowser.get())
          ->Load(UrlLoadParams::InCurrentTab(copiedURL));
    }
  }];
}

- (void)showFullscreenSigninPromoWithCompletion:
    (SigninCoordinatorCompletionCallback)dismissalCompletion {
  DCHECK(!_signinCoordinator)
      << "_signinCoordinator: "
      << base::SysNSStringToUTF8([_signinCoordinator description]);
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  _signinCoordinator = [SigninCoordinator
      fullscreenSigninPromoCoordinatorWithBaseViewController:
          [self activeViewController]
                                                     browser:_regularBrowser
                                                                 .get()
                                                contextStyle:
                                                    SigninContextStyle::kDefault
                           changeProfileContinuationProvider:
                               DoNothingContinuationProvider()];
  [self startSigninCoordinatorWithCompletion:dismissalCompletion];
}

- (void)stopSigninCoordinatorWithCompletionAnimated:(BOOL)animated {
  // The coordinator is retained until the end of the completion, while
  // ensuring that when the completion requests `self` to stop the sign-in
  // coordinator, `stop` is not called a second time.
  SigninCoordinator* signinCoordinator = _signinCoordinator;
  if (!signinCoordinator) {
    return;
  }
  _signinCoordinator = nil;

  [signinCoordinator stopAnimated:animated];
  SigninCoordinatorCompletionCallback signinCompletion =
      signinCoordinator.signinCompletion;
  signinCoordinator.signinCompletion = nil;
  CHECK(signinCompletion);
  // The `signinCoordinator` must be nil here, because `_signinCoordinator`
  // was set to `nil` above.
  signinCompletion(nil, base::unexpected(SigninCoordinatorResultInterrupted));
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  if (!_signinCoordinator) {
    return;
  }
  [self stopSigninCoordinatorWithCompletionAnimated:YES];
  base::UmaHistogramBoolean(
      "Enterprise.BrowserSigninIOS.SignInInterruptedByPolicy", true);
  policyWatcher->SignInUIDismissed();
}

#pragma mark - Private

// Returns the view controller currently displayed by the scene.
- (UIViewController*)activeViewController {
  return [self.delegate activeViewControllerForSigninModalHost:self];
}

// Starts observing the regular browser's `PolicyWatcherBrowserAgent`.
- (void)startObservingPolicyWatcher {
  PolicyWatcherBrowserAgent* policyWatcher =
      PolicyWatcherBrowserAgent::FromBrowser(_regularBrowser.get());
  // The agent is only attached by `AttachBrowserAgents()`, which unit tests
  // don't necessarily call.
  if (!policyWatcher) {
    return;
  }
  _policyWatcherObserverBridge =
      std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);
  _policyWatcherObserver = std::make_unique<base::ScopedObservation<
      PolicyWatcherBrowserAgent, PolicyWatcherBrowserAgentObserverBridge>>(
      _policyWatcherObserverBridge.get());
  _policyWatcherObserver->Observe(policyWatcher);
}

// Returns the consistency promo `SigninCoordinator` for the web sign-in, which
// opens `URL` in the new profile if the user switches profile.
- (SigninCoordinator*)webSigninCoordinatorWithBaseViewController:
                          (UIViewController*)baseViewController
                                                             URL:(const GURL&)
                                                                     URL {
  Browser* currentBrowser =
      _regularBrowser->GetSceneState().browserProviderInterface.currentBrowser;
  id<BrowserCoordinatorCommands> browserCoordinatorHandler = HandlerForProtocol(
      currentBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  void (^prepareChangeProfile)() = ^() {
    [browserCoordinatorHandler closeCurrentTab];
  };
  ChangeProfileContinuationProvider provider =
      base::BindRepeating(&CreateChangeProfileOpensURLContinuation, URL);
  return [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:baseViewController
                                                      browser:_regularBrowser
                                                                  .get()
                                                 contextStyle:
                                                     SigninContextStyle::
                                                         kDefault
                                                  accessPoint:signin_metrics::
                                                                  AccessPoint::
                                                                      kWebSignin
                                         confirmChangeProfile:nil
                                         prepareChangeProfile:
                                             prepareChangeProfile
                                         continuationProvider:provider];
}

// Returns `YES` if a sign-in coordinator can be opened by the host. Fails if
// another sign-in coordinator is already opened.
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
    // a double tap (duration less than 1s), or if `_signinCoordinator` is not
    // visible anymore on the screen (duration more than 1s).
    const base::TimeDelta duration =
        base::TimeTicks::Now() - _signinCoordinator.creationTimeTicks;
    base::UmaHistogramTimes("Signin.ShowSigninCoordinatorWhenAlreadyPresent."
                            "DurationBetweenTwoSigninCoordinatorCreation",
                            duration);
  }
  // TODO(crbug.com/40071586): Change this to a CHECK once this invariant is
  // correct.
  DCHECK(!_signinCoordinator)
      << "_signinCoordinator: "
      << base::SysNSStringToUTF8([_signinCoordinator description]);
  return YES;
}

// Starts the sign-in coordinator and sets its completion.
- (void)startSigninCoordinatorWithCompletion:
    (SigninCoordinatorCompletionCallback)completion {
  DCHECK(_signinCoordinator);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(
          _regularBrowser->GetProfile());
  AuthenticationService::ServiceStatus statusService =
      authenticationService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy: {
      signin_metrics::AccessPoint accessPoint = _signinCoordinator.accessPoint;
      if (completion) {
        // The coordinator argument is `nil` because this completion has never
        // been assigned to a signinCoordinator’s `signinCompletion`. It works
        // because the part that check the coordinator value is in the
        // `signinCompletedWithCoordinator:...` below, and so not integrated in
        // the completion function yet.
        completion(nil, base::unexpected(SigninCoordinatorResultDisabled));
      }
      [self stopSigninCoordinatorAnimated:NO];
      id<PolicyChangeCommands> handler = HandlerForProtocol(
          _regularBrowser->GetCommandDispatcher(), PolicyChangeCommands);
      [handler showForceSignedOutPrompt];
      RecordIfNeededSigninFullscreenPromoEvent(
          SigninFullscreenPromoEvents::kPromoCanceledByPolicy, accessPoint);
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

  if (_regularBrowser->GetSceneState().isUIBlocked) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    if (completion) {
      // The coordinator argument is `nil` because this completion has never
      // been assigned to a signinCoordinator’s `signinCompletion`. It works
      // because the part that check the coordinator value is in the
      // `signinCompletedWithCoordinator:...` below, and so not integrated in
      // the completion function yet.
      completion(nil, base::unexpected(SigninCoordinatorResultInterrupted));
    }
    signin_metrics::AccessPoint accessPoint = _signinCoordinator.accessPoint;
    _signinCoordinator = nil;
    RecordIfNeededSigninFullscreenPromoEvent(
        SigninFullscreenPromoEvents::kPromoCanceledByUIBlocked, accessPoint);
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator,
        SigninCoordinatorResultOrIdentity result) {
        [weakSelf signinCompletedWithCoordinator:coordinator
                                          result:std::move(result)
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
  // This ensures that when the `SigninCoordinator` finishes, it is not detected
  // as still presented.
  SigninCoordinator* signinCoordinator = _signinCoordinator;
  _signinCoordinator = nil;
  [signinCoordinator stopAnimated:animated];
}

// Called when the sign-in coordinator finishes.
- (void)signinCompletedWithCoordinator:(SigninCoordinator*)coordinator
                                result:(SigninCoordinatorResultOrIdentity)result
                            completion:(SigninCoordinatorCompletionCallback)
                                           completion {
  CHECK_EQ(coordinator, _signinCoordinator);

  if (completion) {
    completion(coordinator, std::move(result));
  }
  [self stopSigninCoordinatorAnimated:YES];
}

// Starts handling the hosted commands sent through `browser`'s dispatcher.
// No-op if `browser` is null.
- (void)startDispatchingForBrowser:(Browser*)browser {
  if (!browser) {
    return;
  }
  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(SceneSignInCommands)];
}

// Stops handling the hosted commands sent through `browser`'s dispatcher.
// No-op if `browser` is null.
- (void)stopDispatchingForBrowser:(Browser*)browser {
  if (!browser) {
    return;
  }
  [browser->GetCommandDispatcher() stopDispatchingToTarget:self];
}

@end
