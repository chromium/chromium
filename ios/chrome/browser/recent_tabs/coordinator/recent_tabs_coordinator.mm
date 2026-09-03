// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_coordinator.h"

#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/sessions/core/session_id.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/service/sync_service.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/signin/reauth/coordinator/signin_reauth_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/action_factory.h"
#import "ios/chrome/browser/menu/ui_bundled/menu_histograms.h"
#import "ios/chrome/browser/menu/ui_bundled/tab_context_menu_delegate.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_coordinator.h"
#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_mediator.h"
#import "ios/chrome/browser/recent_tabs/coordinator/recent_tabs_menu_helper.h"
#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/recent_tabs/ui/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/sync_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_util.h"
#import "ios/web/public/web_state.h"

@interface RecentTabsCoordinator () <HistorySyncPopupCoordinatorDelegate,
                                     RecentTabsPresentationDelegate,
                                     SigninReauthCoordinatorDelegate,
                                     TabContextMenuDelegate>
// Completion block called once the recentTabsViewController is dismissed.
@property(nonatomic, copy) ProceduralBlock completion;
// Mediator being managed by this Coordinator.
@property(nonatomic, strong) RecentTabsMediator* mediator;
// ViewController being managed by this Coordinator.
@property(nonatomic, strong)
    TableViewNavigationController* recentTabsNavigationController;
@property(nonatomic, strong)
    RecentTabsTableViewController* recentTabsTableViewController;
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;
@property(nonatomic, strong)
    RecentTabsContextMenuHelper* recentTabsContextMenuHelper;
@end

@implementation RecentTabsCoordinator {
  // Coordinator for the history sync opt-in screen that should appear after
  // sign-in.
  HistorySyncPopupCoordinator* _historySyncPopupCoordinator;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<syncer::SyncService> _syncService;
  SigninReauthCoordinator* _reauthCoordinator;
}

- (void)dealloc {
  CHECK(!self.recentTabsNavigationController, base::NotFatalUntil::M150);
  CHECK(!self.recentTabsTableViewController, base::NotFatalUntil::M150);
  CHECK(!self.mediator, base::NotFatalUntil::M150);
  CHECK(!self.sharingCoordinator, base::NotFatalUntil::M150);
  CHECK(!_authenticationService, base::NotFatalUntil::M150);
  CHECK(!_syncService, base::NotFatalUntil::M150);
  CHECK(!_reauthCoordinator, base::NotFatalUntil::M150);
}

#pragma mark - ChromeCoordinator

- (void)start {
  // Initialize and configure RecentTabsTableViewController.
  self.recentTabsTableViewController =
      [[RecentTabsTableViewController alloc] init];
  self.recentTabsTableViewController.browser = self.browser;
  self.recentTabsTableViewController.loadStrategy = self.loadStrategy;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SceneCommands> sceneHandler =
      HandlerForProtocol(dispatcher, SceneCommands);
  self.recentTabsTableViewController.sceneHandler = sceneHandler;
  self.recentTabsTableViewController.presentationDelegate = self;

  self.recentTabsContextMenuHelper =
      [[RecentTabsContextMenuHelper alloc] initWithBrowser:self.browser
                            recentTabsPresentationDelegate:self
                                    tabContextMenuDelegate:self];
  self.recentTabsTableViewController.menuProvider =
      self.recentTabsContextMenuHelper;
  self.recentTabsTableViewController.session =
      self.baseViewController.view.window.windowScene.session;

  // Adds the dismiss button to the navigation bar and hooks it up to `-stop`.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                           target:self
                           action:@selector(dismissButtonTapped)];
  dismissButton.accessibilityIdentifier = kTableViewNavigationDismissButtonId;
  self.recentTabsTableViewController.navigationItem.rightBarButtonItem =
      dismissButton;

  // Initialize and configure RecentTabsMediator. Make sure to use the
  // OriginalProfile since the mediator services need a SignIn
  // manager which is not present in an OffTheRecord Profile.
  DCHECK(!self.mediator);
  ProfileIOS* profile = self.profile;

  sync_sessions::SessionSyncService* sessionSyncService =
      SessionSyncServiceFactory::GetForProfile(profile);
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile);
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  _syncService = SyncServiceFactory::GetForProfile(profile);
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(profile);
  self.mediator =
      [[RecentTabsMediator alloc] initWithSessionSyncService:sessionSyncService
                                                 authService:authService
                                             identityManager:identityManager
                                              restoreService:restoreService
                                               faviconLoader:faviconLoader
                                                 syncService:_syncService];

  // Set the consumer first before calling [self.mediator initObservers] and
  // then [self.mediator configureConsumer].
  self.mediator.consumer = self.recentTabsTableViewController;
  self.recentTabsTableViewController.imageDataSource = self.mediator;
  [self.mediator initObservers];
  [self.mediator configureConsumer];

  // Present RecentTabsNavigationController.
  self.recentTabsNavigationController = [[TableViewNavigationController alloc]
      initWithTable:self.recentTabsTableViewController];
  self.recentTabsNavigationController.toolbarHidden = YES;

  [self.recentTabsNavigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.recentTabsNavigationController.presentationController.delegate =
      self.recentTabsTableViewController;

  self.recentTabsTableViewController.preventUpdates = NO;

  [self.baseViewController
      presentViewController:self.recentTabsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self stopHistorySyncPopupCoordinator];
  [self.recentTabsTableViewController dismissModals];
  self.recentTabsTableViewController.imageDataSource = nil;
  self.recentTabsTableViewController.browser = nil;
  self.recentTabsTableViewController = nil;
  [self.recentTabsNavigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:self.completion];
  [self stopReauthCoordinator];
  self.recentTabsNavigationController = nil;
  self.recentTabsContextMenuHelper = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  _syncService = nullptr;
  _authenticationService = nullptr;
}

#pragma mark - RecentTabsPresentationDelegate

- (void)showPrimaryAccountReauth {
  if (_reauthCoordinator.viewWillPersist) {
    return;
  }
  [self stopReauthCoordinator];

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  CoreAccountInfo account =
      identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    // A sign-out was triggered in the meantime, don't do anything.
    return;
  }
  _reauthCoordinator = [[SigninReauthCoordinator alloc]
      initWithBaseViewController:self.recentTabsTableViewController
                         browser:self.browser
                         account:account
               reauthAccessPoint:signin_metrics::ReauthAccessPoint::
                                     kRecentTabs];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session {
  base::RecordAction(base::UserMetricsAction(
      "MobileRecentTabManagerOpenAllTabsFromOtherDevice"));
  base::UmaHistogramCounts100(
      "Mobile.RecentTabsManager.TotalTabsFromOtherDevicesOpenAll",
      session->tabs.size());

  BOOL inIncognito = self.profile->IsOffTheRecord();
  UrlLoadingBrowserAgent* URLLoader =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  OpenDistantSessionInBackground(session, inIncognito,
                                 GetDefaultNumberOfTabsToLoadSimultaneously(),
                                 URLLoader, self.loadStrategy);

  [self showActiveRegularTabFromRecentTabs];
}

- (void)openTabWithTabRestoreEntryId:(SessionID)sessionId {
  if (!self.browser) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("MobileRecentTabManagerRecentTabOpened"));
  WebStateList* webStateList = self.browser->GetWebStateList();
  web::WebState* activeWebState = webStateList->GetActiveWebState();
  bool is_ntp =
      activeWebState && activeWebState->GetVisibleURL() == kChromeUINewTabURL;
  new_tab_page_uma::RecordNTPAction(
      self.profile->IsOffTheRecord(), is_ntp,
      new_tab_page_uma::ACTION_OPENED_RECENTLY_CLOSED_ENTRY);

  WindowOpenDisposition disposition =
      IsNTPWithoutHistory(activeWebState)
          ? WindowOpenDisposition::CURRENT_TAB
          : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  RestoreTab(sessionId, disposition, self.browser);
  [self showActiveRegularTabFromRecentTabs];
}

- (void)openDistantTab:(const synced_sessions::DistantTab*)distantTab {
  if (!self.browser) {
    return;
  }
  // Shouldn't reach this if in incognito.
  DCHECK(!self.profile->IsOffTheRecord());

  sync_sessions::OpenTabsUIDelegate* openTabs =
      SessionSyncServiceFactory::GetForProfile(self.profile)
          ->GetOpenTabsUIDelegate();
  const sessions::SessionTab* toLoad = nullptr;
  if (openTabs->GetForeignTab(distantTab->session_tag, distantTab->tab_id,
                              &toLoad)) {
    base::TimeDelta time_since_last_use = base::Time::Now() - toLoad->timestamp;
    base::UmaHistogramCustomTimes("IOS.DistantTab.TimeSinceLastUse",
                                  time_since_last_use, base::Minutes(1),
                                  base::Days(24), 50);

    base::RecordAction(base::UserMetricsAction(
        "MobileRecentTabManagerTabFromOtherDeviceOpened"));
    WebStateList* webStateList = self.browser->GetWebStateList();
    web::WebState* currentWebState = webStateList->GetActiveWebState();
    bool is_ntp = currentWebState &&
                  currentWebState->GetVisibleURL() == kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(
        self.profile->IsOffTheRecord(), is_ntp,
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);
    std::unique_ptr<web::WebState> web_state =
        session_util::CreateWebStateWithNavigationEntries(
            self.profile, toLoad->current_navigation_index,
            toLoad->navigations);
    if (IsNTPWithoutHistory(currentWebState)) {
      webStateList->ReplaceWebStateAt(webStateList->active_index(),
                                      std::move(web_state));
    } else {
      webStateList->InsertWebState(
          std::move(web_state),
          WebStateList::InsertionParams::Automatic().Activate());
    }
  }
  [self showActiveRegularTabFromRecentTabs];
}

- (void)showActiveRegularTabFromRecentTabs {
  // Stopping this coordinator reveals the tab UI underneath.
  self.completion = nil;
  [self.delegate recentTabsCoordinatorWantsToBeDismissed:self];
}

- (void)showHistoryFromRecentTabs {
  // Dismiss recent tabs before presenting history.
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SceneCommands> handler = HandlerForProtocol(dispatcher, SceneCommands);
  __weak RecentTabsCoordinator* weakSelf = self;
  self.completion = ^{
    [handler showHistory];
    weakSelf.completion = nil;
  };
  [self.delegate recentTabsCoordinatorWantsToBeDismissed:self];
}

- (void)showHistorySyncOptInAfterDedicatedSignIn:(BOOL)dedicatedSignInDone {
  // Stop the previous coordinator since the user can tap on the promo button
  // to open a new History Sync Page while the dismiss animation of the previous
  // one is in progress.
  [self stopHistorySyncPopupCoordinator];
  // Show the History Sync Opt-In screen. The coordinator will dismiss itself
  // if there is no signed-in account (eg. if sign-in unsuccessful) or if sync
  // is disabled by policies.
  if (history_sync::GetSkipReason(_syncService, _authenticationService,
                                  self.profile->GetPrefs(), NO) !=
      history_sync::HistorySyncSkipReason::kNone) {
    [self.mediator refreshSessionsView];
  } else {
    _historySyncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
        initWithBaseViewController:self.recentTabsTableViewController
                           browser:self.browser
                     showUserEmail:!dedicatedSignInDone
                 signOutIfDeclined:dedicatedSignInDone
                        isOptional:NO
                      contextStyle:SigninContextStyle::kDefault
                       accessPoint:signin_metrics::AccessPoint::kRecentTabs];
    _historySyncPopupCoordinator.delegate = self;
    [_historySyncPopupCoordinator start];
  }
}

- (void)deleteForeignSession:(const std::string&)sessionTag {
  SessionSyncServiceFactory::GetForProfile(self.profile)
      ->GetOpenTabsUIDelegate()
      ->DeleteForeignSession(sessionTag);
}

- (void)didTapPromoActionButton {
  if (!_syncService) {
    return;
  }
  syncer::SyncService::UserActionableError error =
      _syncService->GetUserActionableError();
  if (error == syncer::SyncService::UserActionableError::kSignInNeedsUpdate) {
    [self showPrimaryAccountReauth];
  } else if ([self shouldShowHistorySyncOnPromoAction]) {
    [self showHistorySyncOptInAfterDedicatedSignIn:NO];
  } else if (ShouldShowSyncSettings(error)) {
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    id<SettingsCommands> settingsHandler =
        HandlerForProtocol(dispatcher, SettingsCommands);
    [settingsHandler
        showSyncSettingsFromViewController:self.recentTabsTableViewController];
  } else if (error ==
             syncer::SyncService::UserActionableError::kNeedsPassphrase) {
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    id<SettingsCommands> settingsHandler =
        HandlerForProtocol(dispatcher, SettingsCommands);
    [settingsHandler showSyncPassphraseSettingsFromViewController:
                         self.recentTabsTableViewController];
  }
}

#pragma mark - RecentTabsContextMenuDelegate

- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        scenario:(SharingScenario)scenario
        fromView:(UIView*)view {
  SharingParams* params = [[SharingParams alloc] initWithURL:URL
                                                       title:title
                                                    scenario:scenario];
  [self.sharingCoordinator stop];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.recentTabsTableViewController
                         browser:self.browser
                          params:params
                      sourceItem:view];
  [self.sharingCoordinator start];
}

- (void)removeSessionAtTableSectionWithIdentifier:(NSInteger)sectionIdentifier {
  [self.recentTabsTableViewController
      removeSessionAtTableSectionWithIdentifier:sectionIdentifier];
}

- (synced_sessions::DistantSession const*)sessionForTableSectionWithIdentifier:
    (NSInteger)sectionIdentifier {
  return [self.recentTabsTableViewController
      sessionForTableSectionWithIdentifier:sectionIdentifier];
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(HistorySyncResult)result {
  [self stopHistorySyncPopupCoordinator];
  [self.mediator refreshSessionsView];
}

#pragma mark - SigninReauthCoordinatorDelegate

- (void)reauthFinishedWithResult:(ReauthResult)result
                          gaiaID:(const GaiaId*)gaiaID {
  [self stopReauthCoordinator];
}

#pragma mark - Private

// Returns YES if the History Sync Opt-In should be shown when the promo action
// button is tapped.
- (BOOL)shouldShowHistorySyncOnPromoAction {
  // In case it's not necessary to show the history opt-in, but the promo action
  // button is still available, sync errors should be checked to show the
  // correct screen to handle the error (ex. passphrase screen).
  return history_sync::GetSkipReason(_syncService, _authenticationService,
                                     self.profile->GetPrefs(), NO) ==
         history_sync::HistorySyncSkipReason::kNone;
}

- (void)dismissButtonTapped {
  base::RecordAction(base::UserMetricsAction("MobileRecentTabsClose"));
  [self.delegate recentTabsCoordinatorWantsToBeDismissed:self];
}

- (void)stopHistorySyncPopupCoordinator {
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator.delegate = nil;
  _historySyncPopupCoordinator = nil;
}

- (void)stopReauthCoordinator {
  _reauthCoordinator.delegate = nil;
  [_reauthCoordinator stop];
  _reauthCoordinator = nil;
}


@end
