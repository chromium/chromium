// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"

#import "base/ios/block_types.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_provider.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

@interface RecentTabsCoordinator () <HistorySyncPopupCoordinatorDelegate,
                                     RecentTabsPresentationDelegate,
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
}

- (void)start {
  // Initialize and configure RecentTabsTableViewController.
  self.recentTabsTableViewController =
      [[RecentTabsTableViewController alloc] init];
  self.recentTabsTableViewController.browser = self.browser;
  self.recentTabsTableViewController.loadStrategy = self.loadStrategy;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  self.recentTabsTableViewController.applicationHandler = applicationHandler;
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  self.recentTabsTableViewController.settingsHandler = settingsHandler;
  self.recentTabsTableViewController.presentationDelegate = self;

  self.recentTabsContextMenuHelper =
      [[RecentTabsContextMenuHelper alloc] initWithBrowser:self.browser
                            recentTabsPresentationDelegate:self
                                    tabContextMenuDelegate:self];
  self.recentTabsTableViewController.menuProvider =
      self.recentTabsContextMenuHelper;
  self.recentTabsTableViewController.session =
      self.baseViewController.view.window.windowScene.session;

  // Adds the "Done" button and hooks it up to `stop`.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [dismissButton
      setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  self.recentTabsTableViewController.navigationItem.rightBarButtonItem =
      dismissButton;

  // Initialize and configure RecentTabsMediator. Make sure to use the
  // OriginalProfile since the mediator services need a SignIn
  // manager which is not present in an OffTheRecord Profile.
  DCHECK(!self.mediator);
  ProfileIOS* profile = self.browser->GetProfile();
  sync_sessions::SessionSyncService* syncService =
      SessionSyncServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile);
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile);
  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
  SceneState* currentSceneState = self.browser->GetSceneState();
  BOOL isDisabled = IsIncognitoModeForced(profile->GetPrefs());
  self.mediator = [[RecentTabsMediator alloc]
      initWithSessionSyncService:syncService
                 identityManager:identityManager
                  restoreService:restoreService
                   faviconLoader:faviconLoader
                     syncService:service
                     browserList:browserList
                      sceneState:currentSceneState
                disabledByPolicy:isDisabled
               engagementTracker:feature_engagement::TrackerFactory::
                                     GetForProfile(profile)
                      modeHolder:nil];

  // Set the consumer first before calling [self.mediator initObservers] and
  // then [self.mediator configureConsumer].
  self.mediator.consumer = self.recentTabsTableViewController;
  self.recentTabsTableViewController.imageDataSource = self.mediator;
  self.recentTabsTableViewController.delegate = self.mediator;
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
  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  [self.recentTabsTableViewController dismissModals];
  self.recentTabsTableViewController.imageDataSource = nil;
  self.recentTabsTableViewController.browser = nil;
  self.recentTabsTableViewController.delegate = nil;
  self.recentTabsTableViewController = nil;
  [self.recentTabsNavigationController
      dismissViewControllerAnimated:YES
                         completion:self.completion];
  self.recentTabsNavigationController = nil;
  self.recentTabsContextMenuHelper = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  [self.mediator disconnect];
}

- (void)dismissButtonTapped {
  base::RecordAction(base::UserMetricsAction("MobileRecentTabsClose"));
  [self.delegate recentTabsCoordinatorWantsToBeDismissed:self];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session {
  base::RecordAction(base::UserMetricsAction(
      "MobileRecentTabManagerOpenAllTabsFromOtherDevice"));
  base::UmaHistogramCounts100(
      "Mobile.RecentTabsManager.TotalTabsFromOtherDevicesOpenAll",
      session->tabs.size());

  BOOL inIncognito = self.browser->GetProfile()->IsOffTheRecord();
  UrlLoadingBrowserAgent* URLLoader =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);
  OpenDistantSessionInBackground(session, inIncognito,
                                 GetDefaultNumberOfTabsToLoadSimultaneously(),
                                 URLLoader, self.loadStrategy);

  [self showActiveRegularTabFromRecentTabs];
}

- (void)showActiveRegularTabFromRecentTabs {
  // Stopping this coordinator reveals the tab UI underneath.
  self.completion = nil;
  [self.delegate recentTabsCoordinatorWantsToBeDismissed:self];
}

- (void)showHistoryFromRecentTabsFilteredBySearchTerms:(NSString*)searchTerms {
  // Dismiss recent tabs before presenting history.
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<ApplicationCommands> handler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
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
  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  // Show the History Sync Opt-In screen. The coordinator will dismiss itself
  // if there is no signed-in account (eg. if sign-in unsuccessful) or if sync
  // is disabled by policies.
  _historySyncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
      initWithBaseViewController:self.recentTabsTableViewController
                         browser:self.browser
                   showUserEmail:!dedicatedSignInDone
               signOutIfDeclined:dedicatedSignInDone
                      isOptional:NO
                     accessPoint:signin_metrics::AccessPoint::
                                     ACCESS_POINT_RECENT_TABS];
  _historySyncPopupCoordinator.delegate = self;
  [_historySyncPopupCoordinator start];
}

#pragma mark - RecentTabsContextMenuDelegate

- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        scenario:(SharingScenario)scenario
        fromView:(UIView*)view {
  SharingParams* params = [[SharingParams alloc] initWithURL:URL
                                                       title:title
                                                    scenario:scenario];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.recentTabsTableViewController
                         browser:self.browser
                          params:params
                      originView:view];
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
                didFinishWithResult:(SigninCoordinatorResult)result {
  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  [self.mediator refreshSessionsView];
}

@end
