// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_coordinator.h"

#import "base/cancelable_callback.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#import "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#import "components/supervised_user/core/browser/proto_fetcher_status.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/deferred_initialization_task_names.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/ai_prototyping/coordinator/ai_prototyping_coordinator.h"
#import "ios/chrome/browser/app_bar/coordinator/app_bar_coordinator.h"
#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_load_url.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin_notification_infobar_delegate.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_factory.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_coordinator.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_coordinator_delegate.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service.h"
#import "ios/chrome/browser/mailto_handler/model/mailto_handler_service_factory.h"
#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/model/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/safari_data_import/coordinator/safari_data_import_main_coordinator.h"
#import "ios/chrome/browser/safari_data_import/model/features.h"
#import "ios/chrome/browser/safari_data_import/public/safari_data_import_entry_point.h"
#import "ios/chrome/browser/scene/ui/scene_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_checkup/password_checkup_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_util.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator.h"
#import "ios/chrome/browser/youtube_incognito/coordinator/youtube_incognito_coordinator_delegate.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_data.h"

namespace {

// The App Store page for Google Chrome.
NSString* const kChromeAppStoreURL = @"https://apps.apple.com/app/id535886823";

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

using UserFeedbackDataCallback =
    base::RepeatingCallback<void(UserFeedbackData*)>;

// Updates `data` with the Family Link member role associated to the primary
// signed-in account, no-op if the account is not enrolled in Family Link.
// TODO(crbug.com/429350831): Factor Family Link code out of SceneCoordinator if
// possible.
void OnListFamilyMembersResponse(
    const GaiaId& primary_account_gaia,
    UserFeedbackData* data,
    const supervised_user::ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ListMembersResponse> response) {
  if (!status.IsOk()) {
    return;
  }
  for (const kidsmanagement::FamilyMember& member : response->members()) {
    if (member.user_id() == primary_account_gaia.ToString()) {
      data.familyMemberRole = base::SysUTF8ToNSString(
          supervised_user::FamilyRoleToString(member.role()));
      break;
    }
  }
}

}  // namespace

@interface SceneCoordinator () <AccountMenuCoordinatorDelegate,
                                HistoryCoordinatorDelegate,
                                IncognitoInterstitialCoordinatorDelegate,
                                PasswordCheckupCoordinatorDelegate,
                                PolicyWatcherBrowserAgentObserving,
                                SafariDataImportMainCoordinatorDelegate,
                                SettingsNavigationControllerDelegate,
                                YoutubeIncognitoCoordinatorDelegate>

// The SceneState for this scene.
@property(nonatomic, readonly) SceneState* sceneState;

// The profile for this scene.
@property(nonatomic, readonly) ProfileIOS* profile;

// The Browser for the current interface.
@property(nonatomic, readonly) Browser* currentBrowser;

@end

@implementation SceneCoordinator {
  id<TabOpening> _tabOpener;
  base::WeakPtr<Browser> _inactiveBrowser;
  base::WeakPtr<Browser> _regularBrowser;
  // Coordinator for the Tab Grid
  TabGridCoordinator* _tabGridCoordinator;
  // Coordinator for the AppBar.
  AppBarCoordinator* _appBarCoordinator;
  // Coordinator for the account menu.
  AccountMenuCoordinator* _accountMenuCoordinator;
  // Coordinator for the sign-in flow.
  SigninCoordinator* _signinCoordinator;
  // Coordinator for the Safari Data Import flow.
  SafariDataImportMainCoordinator* _safariDataImportCoordinator;
  // Coordinator for display of the Password Checkup.
  PasswordCheckupCoordinator* _passwordCheckupCoordinator;
  // Coordinator for displaying history.
  HistoryCoordinator* _historyCoordinator;
  // Coordinator for the Youtube Incognito interstitial.
  YoutubeIncognitoCoordinator* _youtubeIncognitoCoordinator;
  // Coordinator for the Incognito interstitial.
  IncognitoInterstitialCoordinator* _incognitoInterstitialCoordinator;
  // Coordinator for the AI prototyping menu.
  AIPrototypingCoordinator* _AIPrototypingCoordinator;
  // The coordinator for the AIM Assistant.
  AssistantAIMCoordinator* _assistantAIMCoordinator;
  // The coordinator for the Assistant Container.
  AssistantContainerCoordinator* _assistantContainerCoordinator;
  // Observer for PolicyWatcherBrowserAgent.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
  std::unique_ptr<
      base::ScopedObservation<PolicyWatcherBrowserAgent,
                              PolicyWatcherBrowserAgentObserverBridge>>
      _policyWatcherObserver;
  // YES if the Settings view is being dismissed.
  BOOL _dismissingSettings;
  // The view controller to use as a the rootViewController for this scene's
  // window.
  SceneViewController* _viewController;
  // Fetches the Family Link member role asynchronously from KidsManagement API.
  std::unique_ptr<supervised_user::ListFamilyMembersFetcher>
      _familyMembersFetcher;
  // Closure to timeout the request to list family members.
  base::CancelableOnceClosure _familyMembersTimeoutClosure;
  // Navigation View controller for the settings.
  SettingsNavigationController* _settingsNavigationController;
}

- (instancetype)initWithTabOpener:(id<TabOpening>)tabOpener {
  if ((self = [super init])) {
    _tabOpener = tabOpener;
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
      initWithSceneCommandsEndpoint:self
                     regularBrowser:_regularBrowser.get()
                    inactiveBrowser:_inactiveBrowser.get()
                   incognitoBrowser:_incognitoBrowser];
  _tabGridCoordinator.delegate = self.tabGridDelegate;
  [_tabGridCoordinator start];
  if (IsUseSceneViewControllerEnabled()) {
    _viewController = [[SceneViewController alloc] init];
    UIViewController* tabGridViewController =
        _tabGridCoordinator.viewController;
    [_viewController addChildViewController:tabGridViewController];
    [_viewController.appContainer addSubview:tabGridViewController.view];
    tabGridViewController.view.frame = _viewController.appContainer.bounds;
    [tabGridViewController didMoveToParentViewController:_viewController];
    self.sceneState.window.rootViewController = _viewController;
  }

  if (IsChromeNextIaEnabled()) {
    _appBarCoordinator =
        [[AppBarCoordinator alloc] initWithRegularBrowser:_regularBrowser.get()
                                         incognitoBrowser:_incognitoBrowser];
    [_appBarCoordinator start];
    [_viewController setAppBar:_appBarCoordinator.viewController];
  }

  if (IsAssistantContainerEnabled()) {
    _assistantContainerCoordinator = [[AssistantContainerCoordinator alloc]
        initWithBaseViewController:self.activeViewController
                           browser:_regularBrowser.get()];
    [_assistantContainerCoordinator start];
  }
}

- (void)stop {
  // Force close the settings if open. This gives Settings the opportunity to
  // unregister observers and destroy C++ objects before the application is
  // shut down without depending on non-deterministic call to -dealloc.
  [self stopSettingsAnimated:NO completion:nil];
  [_regularBrowser->GetCommandDispatcher() stopDispatchingToTarget:self];
  _policyWatcherObserver.reset();
  _policyWatcherObserverBridge.reset();
  [self stopAccountMenu];
  [self stopSigninCoordinatorWithCompletionAnimated:NO];
  [self stopSafariDataImportCoordinator];
  [self stopPasswordCheckupCoordinator];
  [self stopHistoryCoordinator];
  [self stopYoutubeIncognitoCoordinator];
  [self stopIncognitoInterstitialCoordinator];
  [_AIPrototypingCoordinator stop];
  _AIPrototypingCoordinator = nil;
  [self stopAssistantAIMCoordinator];
  [self stopAssistantContainerCoordinator];
  [_tabGridCoordinator stop];
  [_appBarCoordinator stop];
  self.UIHandler = nil;
  self.tabGridDelegate = nil;
  self.sceneURLLoadingService = nullptr;
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
  if (_settingsNavigationController) {
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

- (void)openNonIncognitoTab:(ProceduralBlock)completion {
  if (_regularBrowser->GetWebStateList()->GetActiveWebState()) {
    // Reuse an existing tab, if one exists.
    ApplicationMode mode = [self isIncognitoForced] ? ApplicationMode::INCOGNITO
                                                    : ApplicationMode::NORMAL;
    [self.UIHandler setCurrentInterfaceForMode:mode];
    if (self.isTabGridActive) {
      [self.UIHandler displayCurrentBVC:completion];
    } else {
      if (completion) {
        completion();
      }
    }
  } else {
    // Open a new NTP.
    UrlLoadParams params = UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
    params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
    ApplicationModeForTabOpening mode =
        [self isIncognitoForced] ? ApplicationModeForTabOpening::INCOGNITO
                                 : ApplicationModeForTabOpening::NORMAL;
    [_tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:mode
                                         withUrlLoadParams:params
                                            dismissOmnibox:YES
                                                completion:completion];
  }
}

- (void)showTabGridPage:(TabGridPage)page {
  [_tabGridCoordinator showTabGridPage:page];
}

- (void)showBrowserLayoutViewController:
            (BrowserLayoutViewController*)viewController
                              incognito:(BOOL)incognito
                             completion:(ProceduralBlock)completion {
  [_tabGridCoordinator showBrowserLayoutViewController:viewController
                                             incognito:incognito
                                            completion:completion];
}

- (void)setActiveMode:(TabGridMode)mode {
  [_tabGridCoordinator setActiveMode:mode];
}

- (void)showYoutubeIncognitoWithUrlLoadParams:
    (const UrlLoadParams&)URLLoadParams {
  _youtubeIncognitoCoordinator = [[YoutubeIncognitoCoordinator alloc]
      initWithBaseViewController:self.activeViewController
                         browser:self.currentBrowser];
  _youtubeIncognitoCoordinator.delegate = self;
  _youtubeIncognitoCoordinator.tabOpener = _tabOpener;
  _youtubeIncognitoCoordinator.urlLoadParams = URLLoadParams;
  _youtubeIncognitoCoordinator.incognitoDisabled =
      [self isIncognitoModeDisabled];
  [_youtubeIncognitoCoordinator start];
}

- (void)showIncognitoInterstitialWithUrlLoadParams:
    (const UrlLoadParams&)URLLoadParams {
  DCHECK(_incognitoInterstitialCoordinator == nil);
  _incognitoInterstitialCoordinator = [[IncognitoInterstitialCoordinator alloc]
      initWithBaseViewController:self.activeViewController
                         browser:self.currentBrowser];
  _incognitoInterstitialCoordinator.delegate = self;
  _incognitoInterstitialCoordinator.tabOpener = _tabOpener;
  _incognitoInterstitialCoordinator.urlLoadParams = URLLoadParams;
  [_incognitoInterstitialCoordinator start];
}

#pragma mark - SceneCommands

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion {
  [self dismissModalDialogsWithCompletion:completion
                           dismissOmnibox:YES
                         dismissSnackbars:YES];
}

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox
                         dismissSnackbars:(BOOL)dismissSnackbars {
  // Disconnected scenes should no-op, since browser objects may not exist.
  // See crbug.com/371847600.
  if (self.sceneState.activationLevel == SceneActivationLevelDisconnected) {
    return;
  }
  // During startup, there may be no current browser. Do nothing in that
  // case.
  if (!self.currentBrowser) {
    return;
  }

  // Immediately hide modals from the provider (alert views, action sheets,
  // popovers). They will be ultimately dismissed by their owners, but at least,
  // they are not visible.
  ios::provider::HideModalViewStack();

  // Conditionally dismiss all snackbars.
  if (dismissSnackbars) {
    id<SnackbarCommands> snackbarHandler = HandlerForProtocol(
        _regularBrowser->GetCommandDispatcher(), SnackbarCommands);
    [snackbarHandler dismissAllSnackbars];
  }

  // Exit fullscreen mode for web page when we re-enter app through external
  // intents.
  web::WebState* webState =
      _regularBrowser->GetWebStateList()->GetActiveWebState();
  if (webState && webState->IsWebPageInFullscreenMode()) {
    webState->CloseMediaPresentations();
  }

  // ChromeIdentityService is responsible for the dialogs displayed by the
  // services it wraps.
  GetApplicationContext()->GetSystemIdentityManager()->DismissDialogs();

  // MailtoHandlerService is responsible for the dialogs displayed by the
  // services it wraps.
  MailtoHandlerServiceFactory::GetForProfile(self.currentBrowser->GetProfile())
      ->DismissAllMailtoHandlerInterfaces();

  id<BookmarksCommands> bookmarksHandler = HandlerForProtocol(
      _regularBrowser->GetCommandDispatcher(), BookmarksCommands);
  [bookmarksHandler dismissBookmarkModalControllerAnimated:NO];

  id<BrowserCoordinatorCommands> browserCoordinatorHandler = HandlerForProtocol(
      self.currentBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  ProceduralBlock completionWithBVC = ^{
    DCHECK(!self.isTabGridActive);
    DCHECK(!self.isSigninInProgress);
    [browserCoordinatorHandler
        clearPresentedStateWithCompletion:completion
                           dismissOmnibox:dismissOmnibox];
  };
  ProceduralBlock completionWithoutBVC = ^{
    // The BVC may exist but tab switcher should be active.
    DCHECK(self.isTabGridActive);
    DCHECK(!self.isSigninInProgress);
    [self stopChildCoordinatorsWithCompletion:completion];
  };

  // Select a completion based on whether the BVC is shown.
  ProceduralBlock chosenCompletion =
      self.isTabGridActive ? completionWithoutBVC : completionWithBVC;

  [self closePresentedViews:NO completion:chosenCompletion];

  // Verify that no modal views are left presented.
  ios::provider::LogIfModalViewsArePresented();
}

- (void)dismissModalsAndShowPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  __weak SceneCoordinator* weakSelf = self;
  [self dismissModalDialogsWithCompletion:^{
    [weakSelf showPasswordCheckupPageForReferrer:referrer];
  }];
}

- (void)
    showPasswordIssuesWithWarningType:(password_manager::WarningType)warningType
                             referrer:(password_manager::PasswordCheckReferrer)
                                          referrer {
  [self startPasswordCheckupCoordinator:referrer];
  [_passwordCheckupCoordinator showPasswordIssuesWithWarningType:warningType];
  [self presentSettingsFromViewController:self.activeViewController];
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
  if (_settingsNavigationController) {
    DCHECK(_settingsNavigationController.presentingViewController)
        << base::SysNSStringToUTF8(
               [_settingsNavigationController.viewControllers description]);
    return;
  }
  [self.sceneState.profileState.appState.deferredRunner
      runBlockNamed:kStartupInitPrefObservers];

  Browser* browser = _regularBrowser.get();

  _settingsNavigationController = [SettingsNavigationController
      mainSettingsControllerForBrowser:browser
                              delegate:self
              hasDefaultBrowserBlueDot:hasDefaultBrowserBlueDot];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showPriceTrackingNotificationsSettings {
  CHECK(!self.isSigninInProgress);
  if (_settingsNavigationController) {
    __weak SceneCoordinator* weakSelf = self;
    [self closePresentedViews:NO
                   completion:^{
                     [weakSelf openPriceTrackingNotificationsSettings];
                   }];
    return;
  }
  [self openPriceTrackingNotificationsSettings];
}

- (void)showSafeBrowsingSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (_settingsNavigationController) {
    [_settingsNavigationController showSafeBrowsingSettings];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      safeBrowsingControllerForBrowser:_regularBrowser.get()
                              delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)stopAllVoiceSearch {
  // Stop voice search on the regular browser.
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      _regularBrowser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [handler stopVoiceSearch];

  // Stop voice search on the incognito browser.
  handler = HandlerForProtocol(_incognitoBrowser->GetCommandDispatcher(),
                               BrowserCoordinatorCommands);
  [handler stopVoiceSearch];
}

- (void)showHistory {
  CHECK(!self.currentBrowser->GetProfile()->IsOffTheRecord())
      << "Current interface is incognito and should NOT show history. Call "
         "this on regular interface.";
  _historyCoordinator = CreateHistoryCoordinator(self.activeViewController,
                                                 _regularBrowser.get());
  _historyCoordinator.loadStrategy = UrlLoadStrategy::NORMAL;
  _historyCoordinator.delegate = self;
  [_historyCoordinator start];
}

- (void)showAssistant {
  if (!IsAssistantContainerEnabled()) {
    return;
  }
  [_assistantAIMCoordinator stop];
  _assistantAIMCoordinator = [[AssistantAIMCoordinator alloc]
      initWithBaseViewController:self.activeViewController
                         browser:self.currentBrowser];
  [_assistantAIMCoordinator start];
}

- (void)closePresentedViewsAndOpenURL:(OpenNewTabCommand*)command {
  DCHECK([command fromChrome]);
  UrlLoadParams params = UrlLoadParams::InNewTab([command URL]);
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  id<TabOpening> tabOpener = _tabOpener;
  ProceduralBlock completion = ^{
    ApplicationModeForTabOpening mode =
        [self isIncognitoForced] ? ApplicationModeForTabOpening::INCOGNITO
                                 : ApplicationModeForTabOpening::NORMAL;
    [tabOpener dismissModalsAndMaybeOpenSelectedTabInMode:mode
                                        withUrlLoadParams:params
                                           dismissOmnibox:YES
                                               completion:nil];
  };
  [self closePresentedViews:YES completion:completion];
}

- (void)closePresentedViews {
  [self closePresentedViews:YES completion:nil];
}

- (void)prepareTabSwitcher {
  web::WebState* currentWebState =
      self.currentBrowser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    SnapshotTabHelper::FromWebState(currentWebState)
        ->UpdateSnapshotWithCallback(nil);
  }
}

- (void)displayTabGridInMode:(TabGridOpeningMode)mode {
  if (self.isTabGridActive) {
    return;
  }

  BOOL incognito = self.currentBrowser->type() == Browser::Type::kIncognito;
  if (mode == TabGridOpeningMode::kRegular && incognito) {
    [self.UIHandler setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  } else if (mode == TabGridOpeningMode::kIncognito && !incognito) {
    [self.UIHandler setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  }

  [self showTabSwitcher];
}

- (void)showPrivacySettingsFromViewController:
    (UIViewController*)baseViewController {
  if (_settingsNavigationController) {
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      privacyControllerForBrowser:_regularBrowser.get()
                         delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showReportAnIssueFromViewController:
            (UIViewController*)baseViewController
                                     sender:(UserFeedbackSender)sender {
  [self showReportAnIssueFromViewController:baseViewController
                                     sender:sender
                        specificProductData:nil];
}

- (void)
    showReportAnIssueFromViewController:(UIViewController*)baseViewController
                                 sender:(UserFeedbackSender)sender
                    specificProductData:(NSDictionary<NSString*, NSString*>*)
                                            specificProductData {
  DCHECK(baseViewController);
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  __weak SceneCoordinator* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    // Set the delay timeout to capture about 85% of users (approx. 2 seconds),
    // see Signin.ListFamilyMembersRequest.OverallLatency.
    [weakSelf presentReportAnIssueViewController:baseViewController
                                          sender:sender
                             specificProductData:specificProductData
                                         timeout:base::Seconds(2)
                                      completion:base::DoNothing()];
  });
}

- (void)openURLInNewTab:(OpenNewTabCommand*)command {
  if (command.inIncognito) {
    IncognitoReauthSceneAgent* reauthAgent =
        [IncognitoReauthSceneAgent agentFromScene:self.sceneState];
    if (reauthAgent.authenticationRequired) {
      __weak __typeof(self) weakSelf = self;
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success) {
              [weakSelf openURLInNewTab:command];
            }
          }];
      return;
    }
  }

  UrlLoadParams params =
      UrlLoadParams::InNewTab(command.URL, command.virtualURL);
  params.SetInBackground(command.inBackground);
  params.web_params.referrer = command.referrer;
  params.web_params.extra_headers = [command.extraHeaders copy];
  params.in_incognito = command.inIncognito;
  params.append_to = command.appendTo;
  params.origin_point = command.originPoint;
  params.from_chrome = command.fromChrome;
  params.user_initiated = command.userInitiated;
  params.should_focus_omnibox = command.shouldFocusOmnibox;
  params.inherit_opener = !command.inBackground;
  self.sceneURLLoadingService->LoadUrlInNewTab(params);
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

- (void)showAccountMenuFromWebWithURL:(const GURL&)URL {
  if (![self isTabAvailableToPresentViewController]) {
    return;
  }
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

- (void)showWebSigninPromoFromViewController:(UIViewController*)viewController
                                         URL:(const GURL&)URL {
  // Do not display the web sign-in promo if there is any UI on the screen.
  if (viewController.presentedViewController ||
      ![self isTabAvailableToPresentViewController]) {
    return;
  }
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

- (void)showSigninAccountNotificationFromViewController:
    (UIViewController*)baseViewController {
  web::WebState* webState =
      _regularBrowser->GetWebStateList()->GetActiveWebState();
  DCHECK(webState);
  infobars::InfoBarManager* infoBarManager =
      InfoBarManagerImpl::FromWebState(webState);
  DCHECK(infoBarManager);
  CommandDispatcher* dispatcher = _regularBrowser->GetCommandDispatcher();
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  SigninNotificationInfoBarDelegate::Create(
      infoBarManager, self.profile, settingsHandler, baseViewController);
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  self.sceneState.incognitoState.incognitoContentVisible =
      incognitoContentVisible;
}

- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity {
  if (!base::ios::IsMultipleScenesSupported()) {
    return;  // silent no-op.
  }

  ProfileIOS* profile = self.profile;
  if (!profile) {
    return;
  }

  UIWindowSceneActivationRequestOptions* options =
      [[UIWindowSceneActivationRequestOptions alloc] init];
  options.requestingScene = self.sceneState.scene;
  if (@available(iOS 26.0, *)) {
    // For iOS26 windowing, ensure the new window doesn't fully overlap the
    // prior window.
    options.placement = [UIWindowSceneProminentPlacement prominentPlacement];
  }

  AttachProfileNameToActivity(userActivity, profile->GetProfileName());
  PrefService* prefs = profile->GetPrefs();
  if (IsIncognitoModeForced(prefs)) {
    userActivity = AdaptUserActivityToIncognito(userActivity, true);
  } else if (IsIncognitoModeDisabled(prefs)) {
    userActivity = AdaptUserActivityToIncognito(userActivity, false);
  }

  [UIApplication.sharedApplication
      requestSceneSessionActivation:nil /* make a new scene */
                       userActivity:userActivity
                            options:options
                       errorHandler:nil];
}

- (void)prepareToPresentModalWithSnackbarDismissal:(BOOL)dismissSnackbars
                                        completion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock ensureNTP = ^{
    [weakSelf ensureNTP];
    completion();
  };
  if (self.isTabGridActive ||
      ((self.currentBrowser->type() == Browser::Type::kIncognito) &&
       ![self isIncognitoForced])) {
    [self closePresentedViews:YES
                   completion:^{
                     [weakSelf openNonIncognitoTab:ensureNTP];
                   }];
    return;
  }
  [self dismissModalDialogsWithCompletion:ensureNTP
                           dismissOmnibox:YES
                         dismissSnackbars:dismissSnackbars];
}

- (void)openAIMenu {
  DCHECK(self.currentBrowser);
  _AIPrototypingCoordinator = [[AIPrototypingCoordinator alloc]
      initWithBaseViewController:self.activeViewController
                         browser:self.currentBrowser];

  // Since this is only for internal prototyping, the coordinator remains active
  // once it's been started.
  [_AIPrototypingCoordinator start];
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

- (void)displaySafariDataImportFromEntryPoint:
            (SafariDataImportEntryPoint)entryPoint
                                withUIHandler:
                                    (id<SafariDataImportUIHandler>)UIHandler {
  // If presented over settings, the base view controller is the top presented
  // view controller. Otherwise, it is the active view controller.
  BOOL presentOverSettings = _settingsNavigationController &&
                             entryPoint == SafariDataImportEntryPoint::kSetting;
  UIViewController* baseViewController = presentOverSettings
                                             ? _settingsNavigationController
                                             : self.activeViewController;

  __weak __typeof(self) weakSelf = self;
  auto startImport = ^{
    [weakSelf displaySafariDataImportFromEntryPoint:entryPoint
                                      withUIHandler:UIHandler
                                 baseViewController:baseViewController];
  };
  if (presentOverSettings) {
    startImport();
  } else {
    [self closePresentedViews:YES completion:startImport];
  }
}

- (void)showAppStorePage {
  [[UIApplication sharedApplication]
                openURL:[NSURL URLWithString:kChromeAppStoreURL]
                options:@{}
      completionHandler:nil];
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
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController
                          skipIfUINotAvailable:NO];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
             accountsControllerForBrowser:_regularBrowser.get()
                       baseViewController:baseViewController
                                 delegate:self
                closeSettingsOnAddAccount:YES
                        showSignoutButton:YES
                           showDoneButton:NO
      signoutDismissalByParentCoordinator:NO];

  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showGeminiSettings {
  if (_settingsNavigationController) {
    [_settingsNavigationController showGeminiSettings];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      BWGControllerForBrowser:_regularBrowser.get()
                     delegate:self];

  UIViewController* presenter = self.activeViewController;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:_settingsNavigationController
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
  if (_settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [_settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      googleServicesControllerForBrowser:_regularBrowser.get()
                                delegate:self];

  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showSyncSettingsFromViewController:baseViewController];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      syncSettingsControllerForBrowser:_regularBrowser.get()
                              delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }
  if (self.sceneState.isUIBlocked) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      syncPassphraseControllerForBrowser:_regularBrowser.get()
                                delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  __weak SceneCoordinator* weakSelf = self;
  [self dismissModalDialogsWithCompletion:^{
    [weakSelf showSavedPasswordsSettingsAfterModalDismissFromViewController:
                  baseViewController];
  }];
}

- (void)showPasswordManagerForCredentialImport:(NSUUID*)UUID
    API_AVAILABLE(ios(26.0)) {
  if (!_settingsNavigationController) {
    _settingsNavigationController = [SettingsNavigationController
        credentialImportControllerForBrowser:_regularBrowser.get()
                                    delegate:self
                                        UUID:UUID];
    [self.activeViewController
        presentViewController:_settingsNavigationController
                     animated:YES
                   completion:nil];
    return;
  }

  CHECK(_settingsNavigationController);
  [_settingsNavigationController showPasswordManagerForCredentialImport:UUID];
}

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode {
  if (_settingsNavigationController) {
    [_settingsNavigationController showPasswordDetailsForCredential:credential
                                                         inEditMode:editMode];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      passwordDetailsControllerForBrowser:_regularBrowser.get()
                                 delegate:self
                               credential:credential
                               inEditMode:editMode];
  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

- (void)showAddressDetails:(autofill::AutofillProfile)address
                inEditMode:(BOOL)editMode
     offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  if (_settingsNavigationController) {
    [_settingsNavigationController showAddressDetails:std::move(address)
                                           inEditMode:editMode
                                offerMigrateToAccount:offerMigrateToAccount];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      addressDetailsControllerForBrowser:_regularBrowser.get()
                                delegate:self
                                 address:std::move(address)
                              inEditMode:editMode
                   offerMigrateToAccount:offerMigrateToAccount];
  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

// TODO(crbug.com/41352590) : Do not pass baseViewController through dispatcher.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.isSigninInProgress);
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      autofillProfileControllerForBrowser:_regularBrowser.get()
                                 delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showCreditCardSettings {
  DCHECK(!self.isSigninInProgress);
  if (_settingsNavigationController) {
    [_settingsNavigationController showCreditCardSettings];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      autofillCreditCardControllerForBrowser:_regularBrowser.get()
                                    delegate:self];
  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

- (void)showCreditCardDetails:(autofill::CreditCard)creditCard
                   inEditMode:(BOOL)editMode {
  if (_settingsNavigationController) {
    [_settingsNavigationController showCreditCardDetails:creditCard
                                              inEditMode:editMode];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      autofillCreditCardEditControllerForBrowser:_regularBrowser.get()
                                        delegate:self
                                      creditCard:creditCard
                                      inEditMode:editMode];
  [self.activeViewController presentViewController:_settingsNavigationController
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
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showDefaultBrowserSettingsFromViewController:baseViewController
                                        sourceForUMA:source];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      defaultBrowserControllerForBrowser:_regularBrowser.get()
                                delegate:self
                            sourceForUMA:source];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showAndStartSafetyCheckForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  if (_settingsNavigationController) {
    [_settingsNavigationController showAndStartSafetyCheckForReferrer:referrer];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      safetyCheckControllerForBrowser:_regularBrowser.get()
                             delegate:self
                             referrer:referrer];

  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

- (void)showSafeBrowsingSettings {
  [self showSafeBrowsingSettingsFromViewController:self.activeViewController];
}

- (void)showSafeBrowsingSettingsFromPromoInteraction {
  DCHECK(_settingsNavigationController);
  [_settingsNavigationController showSafeBrowsingSettingsFromPromoInteraction];
}

- (void)showPasswordSearchPage {
  if (_settingsNavigationController) {
    [_settingsNavigationController showPasswordSearchPage];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      passwordManagerSearchControllerForBrowser:_regularBrowser.get()
                                       delegate:self];
  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

- (void)showContentsSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showContentsSettingsFromViewController:baseViewController];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      contentSettingsControllerForBrowser:_regularBrowser.get()
                                 delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showNotificationsSettings {
  [self showNotificationsSettingsAndHighlightClient:std::nullopt];
}

- (void)showNotificationsSettingsAndHighlightClient:
    (std::optional<PushNotificationClientId>)clientID {
  if (_settingsNavigationController) {
    [_settingsNavigationController
        showNotificationsSettingsAndHighlightClient:clientID];
    return;
  }

  _settingsNavigationController = [SettingsNavigationController
      notificationsSettingsControllerForBrowser:_regularBrowser.get()
                                         client:clientID
                                       delegate:self];
  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

#pragma mark - Properties

- (void)setTabGridDelegate:(id<TabGridCoordinatorDelegate>)delegate {
  _tabGridDelegate = delegate;
  _tabGridCoordinator.delegate = delegate;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  _incognitoBrowser = incognitoBrowser;
  _tabGridCoordinator.incognitoBrowser = incognitoBrowser;
  if (IsChromeNextIaEnabled()) {
    _appBarCoordinator.incognitoBrowser = incognitoBrowser;
  }
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

- (BOOL)isSigninInProgress {
  return _signinCoordinator != nil;
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

#pragma mark - IncognitoInterstitialCoordinatorDelegate

- (void)shouldStopIncognitoInterstitial:
    (IncognitoInterstitialCoordinator*)incognitoInterstitial {
  DCHECK(incognitoInterstitial == _incognitoInterstitialCoordinator);
  [self stopIncognitoInterstitialCoordinator];
  [self closePresentedViews];
}

#pragma mark - YoutubeIncognitoCoordinatorDelegate

- (void)shouldStopYoutubeIncognitoCoordinator:
    (YoutubeIncognitoCoordinator*)youtubeIncognitoCoordinator {
  DCHECK(youtubeIncognitoCoordinator == _youtubeIncognitoCoordinator);
  [self stopYoutubeIncognitoCoordinator];
  [self closePresentedViews];
}

#pragma mark - PasswordCheckupCoordinatorDelegate

- (void)passwordCheckupCoordinatorDidRemove:
    (PasswordCheckupCoordinator*)coordinator {
  CHECK_EQ(_passwordCheckupCoordinator, coordinator);
  [self stopPasswordCheckupCoordinator];
}

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  [_historyCoordinator dismissWithCompletion:^{
    if (completion) {
      completion();
    }
    [weakSelf stopHistoryCoordinator];
  }];
}

- (void)closeHistory {
  [self closeHistoryWithCompletion:nil];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  [self closePresentedViews];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closePresentedViews];
}

- (void)settingsWasDismissed {
  [_settingsNavigationController cleanUpSettings];
  _settingsNavigationController = nil;
  [self stopPasswordCheckupCoordinator];
}

#pragma mark - Private

// Callbacks for `stopSettingsAnimated:completion:`. It releases the navigation
// controller and call the completion if it is non nil.
- (void)stopSettingsCallbackWithCompletion:(ProceduralBlock)completion {
  _settingsNavigationController = nil;
  if (completion) {
    completion();
  }
}

// Returns YES if incognito mode is disabled.
- (BOOL)isIncognitoModeDisabled {
  return IsIncognitoModeDisabled(_regularBrowser->GetProfile()->GetPrefs());
}

// Returns whether incognito is forced by policy.
- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(_incognitoBrowser->GetProfile()->GetPrefs());
}

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

  if (_settingsNavigationController) {
    [_settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController];
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      savePasswordsControllerForBrowser:_regularBrowser.get()
                               delegate:self];
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// Stops the Incognito interstitial coordinator.
- (void)stopIncognitoInterstitialCoordinator {
  [_incognitoInterstitialCoordinator stop];
  _incognitoInterstitialCoordinator = nil;
}

// Stops the Youtube Incognito coordinator.
- (void)stopYoutubeIncognitoCoordinator {
  [_youtubeIncognitoCoordinator stop];
  _youtubeIncognitoCoordinator.delegate = nil;
  _youtubeIncognitoCoordinator = nil;
}

// Stops the AssistantContainerCoordinator.
- (void)stopAssistantContainerCoordinator {
  [_assistantContainerCoordinator stopAnimated:NO completion:nil];
  _assistantContainerCoordinator = nil;
}

// Stops the AssistantAIMCoordinator.
- (void)stopAssistantAIMCoordinator {
  [_assistantAIMCoordinator stop];
  _assistantAIMCoordinator = nil;
}

// Stops the History coordinator.
- (void)stopHistoryCoordinator {
  [_historyCoordinator stop];
  _historyCoordinator.delegate = nil;
  _historyCoordinator = nil;
}

// Stops the Safari Data Import coordinator.
- (void)stopSafariDataImportCoordinator {
  [_safariDataImportCoordinator stop];
  _safariDataImportCoordinator = nil;
}

// Stops all child coordinators then calls `completion`. `completion` is called
// whether or not child coordinators exist.
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  [_tabGridCoordinator stopChildCoordinatorsWithCompletion:completion];
}

// Presents the Report an Issue UI.
- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                       specificProductData:(NSDictionary<NSString*, NSString*>*)
                                               specificProductData
                                   timeout:(base::TimeDelta)timeout
                                completion:
                                    (UserFeedbackDataCallback)completion {
  UserFeedbackData* userFeedbackData =
      [self createUserFeedbackDataForSender:sender
                        specificProductData:specificProductData];
  [self presentReportAnIssueViewController:baseViewController
                                    sender:sender
                          userFeedbackData:userFeedbackData
                                   timeout:timeout
                                completion:std::move(completion)];
}

// Presents the Report an Issue UI using `data`.
- (void)presentReportAnIssueViewController:(UIViewController*)baseViewController
                                    sender:(UserFeedbackSender)sender
                          userFeedbackData:(UserFeedbackData*)data
                                   timeout:(base::TimeDelta)timeout
                                completion:
                                    (UserFeedbackDataCallback)completion {
  DCHECK(!self.isSigninInProgress);
  if (_settingsNavigationController) {
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(self.profile);
  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  // Retrieve the Family Link member role for the signed-in account and
  // populates the corresponding `UserFeedbackData` property.
  if (!primary_account.IsEmpty()) {
    __weak SceneCoordinator* weakSelf = self;
    _familyMembersFetcher = supervised_user::FetchListFamilyMembers(
        *identity_manager, self.profile->GetSharedURLLoaderFactory(),
        base::BindOnce(&OnListFamilyMembersResponse, primary_account.gaia, data)
            .Then(base::BindOnce(^{
              [weakSelf presentUserFeedbackViewController:baseViewController
                                     withUserFeedbackData:data
                                               completion:completion];
            })));

    // Timeout the request to list family members.
    _familyMembersTimeoutClosure.Reset(base::BindOnce(^{
      [weakSelf presentUserFeedbackViewController:baseViewController
                             withUserFeedbackData:data
                                       completion:completion];
    }));
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, _familyMembersTimeoutClosure.callback(), timeout);
    return;
  }

  [self presentUserFeedbackViewController:baseViewController
                     withUserFeedbackData:data
                               completion:completion];
}

// Presents the Report an Issue UI using `data`. Cancels the family members
// fetcher, and `_familyMembersTimeoutClosure`.
- (void)presentUserFeedbackViewController:(UIViewController*)baseViewController
                     withUserFeedbackData:(UserFeedbackData*)data
                               completion:(UserFeedbackDataCallback)completion {
  // Cancel any timeout.
  _familyMembersTimeoutClosure.Cancel();
  // Cancel any list family member requests in progress.
  _familyMembersFetcher.reset();

  UserFeedbackConfiguration* configuration =
      [[UserFeedbackConfiguration alloc] init];
  configuration.data = data;
  configuration.sceneHandler = self;
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();

  NSError* error;
  ios::provider::StartUserFeedbackFlow(configuration, baseViewController,
                                       &error);
  UMA_HISTOGRAM_BOOLEAN("IOS.FeedbackKit.UserFlowStartedSuccess", error == nil);
  std::move(completion).Run(data);
}

// Creates a UserFeedbackData object with the given sender and product specific
// data.
- (UserFeedbackData*)createUserFeedbackDataForSender:(UserFeedbackSender)sender
                                 specificProductData:
                                     (NSDictionary<NSString*, NSString*>*)
                                         specificProductData {
  UserFeedbackData* data = [[UserFeedbackData alloc] init];
  data.origin = sender;
  data.currentPageIsIncognito =
      self.currentBrowser->GetProfile()->IsOffTheRecord();

  CGFloat scale = 0.0;
  if (self.isTabGridActive) {
    // For screenshots of the tab switcher we need to use a scale of 1.0 to
    // avoid spending too much time since the tab switcher can have lots of
    // subviews.
    scale = 1.0;
  }

  UIView* lastView = self.activeViewController.view;
  DCHECK(lastView);
  data.currentPageScreenshot = CaptureView(lastView, scale);

  data.productSpecificData = specificProductData;
  return data;
}

// Shows the tab switcher UI.
- (void)showTabSwitcher {
  [self setActiveMode:TabGridMode::kNormal];
  TabGridPage page = (self.currentBrowser->type() == Browser::Type::kIncognito)
                         ? TabGridPageIncognitoTabs
                         : TabGridPageRegularTabs;

  [self showTabGridPage:page];
}

// Ensures that a non-incognito NTP tab is open. If incognito is forced, then
// it will ensure an incognito NTP tab is open.
- (void)ensureNTP {
  // If the tab does not exist, open a new tab.
  UrlLoadParams params = UrlLoadParams::InCurrentTab(GURL(kChromeUINewTabURL));
  ApplicationMode mode =
      (self.currentBrowser->type() == Browser::Type::kIncognito)
          ? ApplicationMode::INCOGNITO
          : ApplicationMode::NORMAL;
  [self.UIHandler openOrReuseTabInMode:mode
                     withUrlLoadParams:params
                   tabOpenedCompletion:nil];
}

// Stops the PasswordCheckupCoordinator.
- (void)stopPasswordCheckupCoordinator {
  [_passwordCheckupCoordinator stop];
  _passwordCheckupCoordinator.delegate = nil;
  _passwordCheckupCoordinator = nil;
}

// Creates the settings navigation controller for the safety check if it doesn't
// exist.
- (void)createSafetyCheckSettingsWithReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  if (_settingsNavigationController) {
    return;
  }
  _settingsNavigationController = [SettingsNavigationController
      safetyCheckControllerForBrowser:_regularBrowser.get()
                             delegate:self
                             referrer:referrer];
}

// Stops the sign-in coordinator actions and dismisses its views either
// with or without animation. Executes its signinCompletion. It’s expected to be
// not already executed.
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

// Shows the settings navigation controller.
- (void)presentSettingsFromViewController:
    (UIViewController*)baseViewController {
  [baseViewController presentViewController:_settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// Stops the settings navigation controller.
- (void)stopSettingsAnimated:(BOOL)animated
                  completion:(ProceduralBlock)completion {
  if (_settingsNavigationController) {
    // Clean-up and then dismiss the view controller if it is presented.
    [_settingsNavigationController cleanUpSettings];
    UIViewController* presentingViewController =
        _settingsNavigationController.presentingViewController;

    __weak __typeof(self) weakSelf = self;
    ProceduralBlock cleanup = ^{
      [weakSelf stopSettingsCallbackWithCompletion:completion];
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

// Opens the price tracking notification settings view.
- (void)openPriceTrackingNotificationsSettings {
  Browser* browser = _regularBrowser.get();
  _settingsNavigationController = [SettingsNavigationController
      priceNotificationsControllerForBrowser:browser
                                    delegate:self];
  [self.activeViewController presentViewController:_settingsNavigationController
                                          animated:YES
                                        completion:nil];
}

// Displays the Safari Data Import, from a `baseViewController`.
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

// Shows the password checkup page.
- (void)showPasswordCheckupPageForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  [self startPasswordCheckupCoordinator:referrer];
  [self presentSettingsFromViewController:self.activeViewController];
}

// Starts the PasswordCheckupCoordinator.
- (void)startPasswordCheckupCoordinator:
    (password_manager::PasswordCheckReferrer)referrer {
  [self createSafetyCheckSettingsWithReferrer:referrer];

  _passwordCheckupCoordinator = [[PasswordCheckupCoordinator alloc]
      initWithBaseNavigationController:_settingsNavigationController
                               browser:_regularBrowser.get()
                          reauthModule:nil
                              referrer:referrer];
  _passwordCheckupCoordinator.delegate = self;
  [_passwordCheckupCoordinator start];
}

// Closes any presented views and calls `completion`.
- (void)closePresentedViews:(BOOL)animated
                 completion:(ProceduralBlock)completion {
  // If the Incognito interstitial is active, stop it.
  [self stopIncognitoInterstitialCoordinator];
  [self stopYoutubeIncognitoCoordinator];

  // If History is active, stop it.
  [self stopHistoryCoordinator];

  // If AIM Assistant is active, stop it.
  [self stopAssistantAIMCoordinator];

  // If the Safari data import workflow is active, stop it.
  [self stopSafariDataImportCoordinator];

  __weak __typeof(self) weakSelf = self;
  ProceduralBlock resetAndDismiss = ^{
    __typeof(self) strongSelf = weakSelf;
    // Cleanup Password Checkup after its UI was dismissed.
    [strongSelf stopPasswordCheckupCoordinator];
    if (completion) {
      completion();
    }
  };

  if (_settingsNavigationController && !_dismissingSettings) {
    _dismissingSettings = YES;
    // `self.signinCoordinator` can be presented on top of the settings, to
    // present the Trusted Vault reauthentication `self.signinCoordinator` has
    // to be closed first.
    // If signinCoordinator is already dismissing, completion execution will
    // happen when it is done animating.
    [self stopSigninCoordinatorWithCompletionAnimated:animated];
    [self stopSettingsAnimated:animated completion:resetAndDismiss];
    _dismissingSettings = NO;
  } else {
    // `self.signinCoordinator` can be presented without settings, from the
    // bookmarks or the recent tabs view.
    [self stopSigninCoordinatorWithCompletionAnimated:animated];
    resetAndDismiss();
  }
}

@end
