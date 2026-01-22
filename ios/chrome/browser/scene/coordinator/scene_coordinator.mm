// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/deferred_initialization_task_names.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_load_url.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
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
                                SafariDataImportMainCoordinatorDelegate,
                                SettingsNavigationControllerDelegate>

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
    (id<SceneCommands>)sceneCommandsEndpoint {
  if ((self = [super init])) {
    _sceneCommandsEndpoint = sceneCommandsEndpoint;
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

  CHECK(_regularBrowser.get());
  CHECK(_inactiveBrowser.get());
  CHECK(_incognitoBrowser);

  _tabGridCoordinator = [[TabGridCoordinator alloc]
      initWithSceneCommandsEndpoint:_sceneCommandsEndpoint
                     regularBrowser:_regularBrowser.get()
                    inactiveBrowser:_inactiveBrowser.get()
                   incognitoBrowser:_incognitoBrowser];
  _tabGridCoordinator.delegate = self.delegate;
  [_tabGridCoordinator start];
}

- (void)stop {
  [_regularBrowser->GetCommandDispatcher() stopDispatchingToTarget:self];
  _policyWatcherObserver.reset();
  _policyWatcherObserverBridge.reset();
  [self stopAccountMenu];
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  [self stopSafariDataImportCoordinator];
  [self stopSettingsAnimated:NO completion:nil];
  [_tabGridCoordinator stop];
}

#pragma mark - Public

- (void)setBrowsersFromProvider:(id<BrowserProviderInterface>)provider {
  _regularBrowser = provider.mainBrowserProvider.browser->AsWeakPtr();
  _inactiveBrowser = provider.mainBrowserProvider.inactiveBrowser->AsWeakPtr();
  _incognitoBrowser = provider.incognitoBrowserProvider.browser;
}

- (BOOL)isTabGridActive {
  return _tabGridCoordinator.isTabGridActive;
}

- (BOOL)isTabAvailableToPresentViewController {
  if (self.isSigninInProgress) {
    return NO;
  }
  if (self.settingsNavigationController) {
    return NO;
  }
  if (self.sceneState.profileState.initStage < ProfileInitStage::kFinal) {
    return NO;
  }
  if (self.sceneState.profileState.currentUIBlocker) {
    return NO;
  }
  if (self.isTabGridActive) {
    return NO;
  }
  return YES;
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
                                              browser:self.currentBrowser
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
                                                     browser:self.currentBrowser
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
  _signinCoordinator = [SigninCoordinator
      consistencyPromoSigninCoordinatorWithBaseViewController:viewController
                                                      browser:
                                                          self.currentBrowser
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
  base::WeakPtr<Browser> regularBrowser = _regularBrowser;
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

- (void)createSafetyCheckSettingsWithReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  if (self.settingsNavigationController) {
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      safetyCheckControllerForBrowser:_regularBrowser.get()
                             delegate:self
                             referrer:referrer];
}

- (void)stopSettingsAnimated:(BOOL)animated
                  completion:(ProceduralBlock)completion {
  if (self.settingsNavigationController) {
    // Dismiss the view controller if it is presented.
    UIViewController* presentingViewController =
        self.settingsNavigationController.presentingViewController;

    __weak __typeof(self) weakSelf = self;
    ProceduralBlock cleanup = ^{
      // Cleanup settings.
      [weakSelf.settingsNavigationController cleanUpSettings];
      weakSelf.settingsNavigationController = nil;
      if (completion) {
        completion();
      }
    };

    if (presentingViewController) {
      [presentingViewController dismissViewControllerAnimated:animated
                                                   completion:cleanup];
    } else {
      cleanup();
    }
  } else if (completion) {
    completion();
  }
}

- (void)presentSettingsFromViewController:
    (UIViewController*)baseViewController {
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)maybeShowSettingsFromViewController {
  if (self.isSigninInProgress) {
    return;
  }
  [self showSettingsFromViewController:nil];
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController {
  BOOL hasDefaultBrowserBlueDot = NO;

  Browser* browser = _regularBrowser.get();
  if (browser) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(self.profile);
    if (tracker) {
      hasDefaultBrowserBlueDot =
          ShouldTriggerDefaultBrowserHighlightFeature(tracker);
    }
  }

  if (hasDefaultBrowserBlueDot) {
    RecordDefaultBrowserBlueDotFirstDisplay();
  }

  [self showSettingsFromViewController:baseViewController
              hasDefaultBrowserBlueDot:hasDefaultBrowserBlueDot];
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController
              hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot {
  if (!baseViewController) {
    baseViewController = self.activeViewController;
  }

  DCHECK(!self.isSigninInProgress);
  if (self.settingsNavigationController) {
    DCHECK(self.settingsNavigationController.presentingViewController)
        << base::SysNSStringToUTF8(
               [self.settingsNavigationController.viewControllers description]);
    return;
  }
  [self.sceneState.profileState.appState.deferredRunner
      runBlockNamed:kStartupInitPrefObservers];

  Browser* browser = _regularBrowser.get();

  self.settingsNavigationController = [SettingsNavigationController
      mainSettingsControllerForBrowser:browser
                              delegate:self
              hasDefaultBrowserBlueDot:hasDefaultBrowserBlueDot];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showPrivacySettingsFromViewController:
    (UIViewController*)baseViewController {
  if (self.settingsNavigationController) {
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      privacyControllerForBrowser:_regularBrowser.get()
                         delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showSafeBrowsingSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showSafeBrowsingSettings];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      safeBrowsingControllerForBrowser:_regularBrowser.get()
                              delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)openPriceTrackingNotificationsSettings {
  Browser* browser = _regularBrowser.get();
  self.settingsNavigationController = [SettingsNavigationController
      priceNotificationsControllerForBrowser:browser
                                    delegate:self];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - SettingsCommands

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showAccountsSettingsFromViewController:
            (UIViewController*)baseViewController
                          skipIfUINotAvailable:(BOOL)skipIfUINotAvailable {
  if (!baseViewController) {
    baseViewController = self.activeViewController;
  }
  if (skipIfUINotAvailable && (baseViewController.presentedViewController ||
                               ![self isTabAvailableToPresentViewController])) {
    return;
  }
  DCHECK(!self.isSigninInProgress);

  if (self.currentBrowser->type() == Browser::Type::kIncognito) {
    NOTREACHED();
  }
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController
                          skipIfUINotAvailable:NO];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
             accountsControllerForBrowser:_regularBrowser.get()
                       baseViewController:baseViewController
                                 delegate:self
                closeSettingsOnAddAccount:YES
                        showSignoutButton:YES
                           showDoneButton:NO
      signoutDismissalByParentCoordinator:NO];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showBWGSettings {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showBWGSettings];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      BWGControllerForBrowser:_regularBrowser.get()
                     delegate:self];

  UIViewController* presenter = self.activeViewController;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:self.settingsNavigationController
                          animated:YES
                        completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (!baseViewController) {
    baseViewController = self.activeViewController;
  }
  if (self.settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [self.settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      googleServicesControllerForBrowser:_regularBrowser.get()
                                delegate:self];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSyncSettingsFromViewController:baseViewController];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      syncSettingsControllerForBrowser:_regularBrowser.get()
                              delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }
  if (self.sceneState.isUIBlocked) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      syncPassphraseControllerForBrowser:_regularBrowser.get()
                                delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  __weak SceneCoordinator* weakSelf = self;
  [_sceneCommandsEndpoint dismissModalDialogsWithCompletion:^{
    [weakSelf showSavedPasswordsSettingsAfterModalDismissFromViewController:
                  baseViewController];
  }];
}

- (void)showPasswordManagerForCredentialImport:(NSUUID*)UUID {
  if (!self.settingsNavigationController) {
    self.settingsNavigationController = [SettingsNavigationController
        credentialImportControllerForBrowser:_regularBrowser.get()
                                    delegate:self
                                        UUID:UUID];
    [self.activeViewController
        presentViewController:self.settingsNavigationController
                     animated:YES
                   completion:nil];
    return;
  }

  CHECK(self.settingsNavigationController);
  [self.settingsNavigationController
      showPasswordManagerForCredentialImport:UUID];
}

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showPasswordDetailsForCredential:credential
                              inEditMode:editMode];
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      passwordDetailsControllerForBrowser:_regularBrowser.get()
                                 delegate:self
                               credential:credential
                               inEditMode:editMode];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showAddressDetails:(autofill::AutofillProfile)address
                inEditMode:(BOOL)editMode
     offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
           showAddressDetails:std::move(address)
                   inEditMode:editMode
        offerMigrateToAccount:offerMigrateToAccount];
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      addressDetailsControllerForBrowser:_regularBrowser.get()
                                delegate:self
                                 address:std::move(address)
                              inEditMode:editMode
                   offerMigrateToAccount:offerMigrateToAccount];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      autofillProfileControllerForBrowser:_regularBrowser.get()
                                 delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showCreditCardSettings {
  DCHECK(!self.isSigninInProgress);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showCreditCardSettings];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardControllerForBrowser:_regularBrowser.get()
                                    delegate:self];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showCreditCardDetails:(autofill::CreditCard)creditCard
                   inEditMode:(BOOL)editMode {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showCreditCardDetails:creditCard
                                                  inEditMode:editMode];
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardEditControllerForBrowser:_regularBrowser.get()
                                        delegate:self
                                      creditCard:creditCard
                                      inEditMode:editMode];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserSettingsPageSource)
                                                source {
  if (!baseViewController) {
    baseViewController = self.activeViewController;
  }
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showDefaultBrowserSettingsFromViewController:baseViewController
                                        sourceForUMA:source];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      defaultBrowserControllerForBrowser:_regularBrowser.get()
                                delegate:self
                            sourceForUMA:source];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showAndStartSafetyCheckForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAndStartSafetyCheckForReferrer:referrer];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      safetyCheckControllerForBrowser:_regularBrowser.get()
                             delegate:self
                             referrer:referrer];

  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showSafeBrowsingSettings {
  [self showSafeBrowsingSettingsFromViewController:self.activeViewController];
}

- (void)showSafeBrowsingSettingsFromPromoInteraction {
  DCHECK(self.settingsNavigationController);
  [self.settingsNavigationController
          showSafeBrowsingSettingsFromPromoInteraction];
}

- (void)showPasswordSearchPage {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController showPasswordSearchPage];
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      passwordManagerSearchControllerForBrowser:_regularBrowser.get()
                                       delegate:self];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)showContentsSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showContentsSettingsFromViewController:baseViewController];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      contentSettingsControllerForBrowser:_regularBrowser.get()
                                 delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showNotificationsSettings {
  [self showNotificationsSettingsAndHighlightClient:std::nullopt];
}

- (void)showNotificationsSettingsAndHighlightClient:
    (std::optional<PushNotificationClientId>)clientID {
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showNotificationsSettingsAndHighlightClient:clientID];
    return;
  }

  self.settingsNavigationController = [SettingsNavigationController
      notificationsSettingsControllerForBrowser:_regularBrowser.get()
                                         client:clientID
                                       delegate:self];
  [self.activeViewController
      presentViewController:self.settingsNavigationController
                   animated:YES
                 completion:nil];
}

#pragma mark - Properties

- (void)setDelegate:(id<SceneCoordinatorDelegate>)delegate {
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

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  id<SceneCommands> sceneHandler = HandlerForProtocol(
      _regularBrowser->GetCommandDispatcher(), SceneCommands);
  [sceneHandler closePresentedViews];
}

- (void)settingsWasDismissed {
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
  [self.delegate sceneCoordinatorDidDismissSettings:self];
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

// Shows the saved passwords settings in the settings UI.
- (void)showSavedPasswordsSettingsAfterModalDismissFromViewController:
    (UIViewController*)baseViewController {
  if (!baseViewController) {
    // TODO(crbug.com/41352590): Don't pass base view controller through
    // dispatched command.
    baseViewController = self.activeViewController;
  }
  DCHECK(!self.isSigninInProgress);

  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController];
    return;
  }
  self.settingsNavigationController = [SettingsNavigationController
      savePasswordsControllerForBrowser:_regularBrowser.get()
                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

@end
