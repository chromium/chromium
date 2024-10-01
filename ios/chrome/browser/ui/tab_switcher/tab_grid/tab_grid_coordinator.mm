// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/home/bookmarks_coordinator.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_prompt_coordinator.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_coordinator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/commerce/ui_bundled/price_card/price_card_mediator.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/util.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator.h"
#import "ios/chrome/browser/history/ui_bundled/history_coordinator_delegate.h"
#import "ios/chrome/browser/history/ui_bundled/public/history_presentation_delegate.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_container_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_coordinator_audience.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_positioner.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_panel_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_animation_layout_providing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// If Find in Page uses the system Find panel and if the Find UI is marked as
// active in the current web state of `browser`, this returns true. Otherwise,
// returns false.
bool FindNavigatorShouldBePresentedInBrowser(Browser* browser) {
  if (!IsNativeFindInPageAvailable() || !browser) {
    return false;
  }

  web::WebState* currentWebState =
      browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return false;
  }

  FindTabHelper* helper = FindTabHelper::FromWebState(currentWebState);
  if (!helper) {
    return false;
  }

  return helper->IsFindUIActive();
}

}  // namespace

@interface TabGridCoordinator () <BringAndroidTabsCommands,
                                  GridCoordinatorAudience,
                                  GridMediatorDelegate,
                                  HistoryCoordinatorDelegate,
                                  HistoryPresentationDelegate,
                                  HistorySyncPopupCoordinatorDelegate,
                                  InactiveTabsCoordinatorDelegate,
                                  LegacyGridTransitionAnimationLayoutProviding,
                                  RecentTabsPresentationDelegate,
                                  SceneStateObserver,
                                  SnackbarCoordinatorDelegate,
                                  TabContextMenuDelegate,
                                  TabGridCommands,
                                  TabGridViewControllerDelegate,
                                  TabGroupPositioner,
                                  TabPresentationDelegate> {
  // Use an explicit ivar instead of synthesizing as the setter isn't using the
  // ivar.
  raw_ptr<Browser> _incognitoBrowser;

  // Browser that contain tabs, from the regular browser, that have not been
  // open since a certain amount of time.
  raw_ptr<Browser> _inactiveBrowser;

  // The coordinator that shows the bookmarking UI after the user taps the Add
  // to Bookmarks button.
  BookmarksCoordinator* _bookmarksCoordinator;

  // The coordinator that manages the "Bring Android Tabs" prompt for Android
  // switchers.
  BringAndroidTabsPromptCoordinator* _bringAndroidTabsPromptCoordinator;

  // Coordinator for the history sync opt-in screen that should appear after
  // sign-in.
  HistorySyncPopupCoordinator* _historySyncPopupCoordinator;

  // Coordinator for the "Tab List From Android Prompt" for Android switchers.
  TabListFromAndroidCoordinator* _tabListFromAndroidCoordinator;

  // Coordinator for the toolbars.
  TabGridToolbarsCoordinator* _toolbarsCoordinator;

  // Mediator of the tab grid.
  TabGridMediator* _mediator;

  // Incognito grid coordinator.
  IncognitoGridCoordinator* _incognitoGridCoordinator;

  // Regular grid coordinator.
  RegularGridCoordinator* _regularGridCoordinator;

  // Tab Groups panel coordinator.
  TabGroupsPanelCoordinator* _tabGroupsPanelCoordinator;

  // Remote grid container.
  // TODO(crbug.com/40273478): To remove when remote coordinator handles it.
  GridContainerViewController* _remoteGridContainerViewController;

  // The frame of the Tab Grid when it is presented.
  CGRect _frameWhenEntering;

  // Holder for the current mode of the whole tab grid.
  TabGridModeHolder* _modeHolder;
}

// Browser that contain tabs from the main pane (i.e. non-incognito).
// TODO(crbug.com/40893775): Make regular ivar as incognito and inactive.
@property(nonatomic, assign, readonly) Browser* regularBrowser;
// Superclass property specialized for the class that this coordinator uses.
@property(nonatomic, weak) TabGridViewController* baseViewController;
// Commad dispatcher used while this coordinator's view controller is active.
@property(nonatomic, strong) CommandDispatcher* dispatcher;
// Container view controller for the BVC to live in; this class's view
// controller will present this.
@property(nonatomic, strong) BVCContainerViewController* bvcContainer;
// Handler for the transitions between the TabGrid and the Browser.
@property(nonatomic, strong)
    LegacyTabGridTransitionHandler* legacyTransitionHandler;
// New handler for the transitions between the TabGrid and the Browser.
@property(nonatomic, strong) TabGridTransitionHandler* transitionHandler;
// Mediator for regular Tabs.
@property(nonatomic, weak) RegularGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, weak) IncognitoGridMediator* incognitoTabsMediator;
// Mediator for PriceCardView - this is only for regular Tabs.
@property(nonatomic, strong) PriceCardMediator* priceCardMediator;
// Mediator for remote Tabs.
@property(nonatomic, strong) RecentTabsMediator* remoteTabsMediator;
// TODO(crbug.com/346302283): Some tests depend on a
// RecentTabsTableViewController to have been loaded and kept in memory.
// Investigate and remove this dependency.
@property(nonatomic, strong)
    RecentTabsTableViewController* hackRecentTabsTableViewController;
// Mediator for the inactive tabs button.
@property(nonatomic, strong)
    InactiveTabsButtonMediator* inactiveTabsButtonMediator;
// Coordinator for history, which can be started from recent tabs.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
// YES if the TabViewController has never been shown yet.
@property(nonatomic, assign) BOOL firstPresentation;
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;
@property(nonatomic, strong)
    RecentTabsContextMenuHelper* recentTabsContextMenuHelper;
// The action sheet coordinator, if one is currently being shown.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;
// Coordinator for snackbar presentation on `_regularBrowser`.
@property(nonatomic, strong) SnackbarCoordinator* snackbarCoordinator;
// Coordinator for snackbar presentation on `_incognitoBrowser`.
@property(nonatomic, strong) SnackbarCoordinator* incognitoSnackbarCoordinator;
// Coordinator for inactive tabs.
@property(nonatomic, strong) InactiveTabsCoordinator* inactiveTabsCoordinator;
// The timestamp of the user entering the tab grid.
@property(nonatomic, assign) base::TimeTicks tabGridEnterTime;

// The page configuration used when create the tab grid view controller;
@property(nonatomic, assign) TabGridPageConfiguration pageConfiguration;

@property(weak, nonatomic, readonly) UIWindow* window;

@end

@implementation TabGridCoordinator
// Superclass property.
@synthesize baseViewController = _baseViewController;
// Ivars are not auto-synthesized when accessors are overridden.
@synthesize regularBrowser = _regularBrowser;

- (instancetype)initWithWindow:(nullable UIWindow*)window
     applicationCommandEndpoint:
         (id<ApplicationCommands>)applicationCommandEndpoint
                 regularBrowser:(Browser*)regularBrowser
                inactiveBrowser:(Browser*)inactiveBrowser
               incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super initWithBaseViewController:nil browser:nullptr])) {
    CHECK(inactiveBrowser->IsInactive());
    CHECK(!regularBrowser->IsInactive());
    _window = window;
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so SettingsCommands is explicitly dispatched
    // to the endpoint as well.
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(SettingsCommands)];

    _regularBrowser = regularBrowser;
    _inactiveBrowser = inactiveBrowser;
    _incognitoBrowser = incognitoBrowser;

    if (IsIncognitoModeDisabled(_regularBrowser->GetProfile()->GetPrefs())) {
      _pageConfiguration = TabGridPageConfiguration::kIncognitoPageDisabled;
    } else if (IsIncognitoModeForced(
                   _incognitoBrowser->GetProfile()->GetPrefs())) {
      _pageConfiguration = TabGridPageConfiguration::kIncognitoPageOnly;
    } else {
      _pageConfiguration = TabGridPageConfiguration::kAllPagesEnabled;
    }
  }
  return self;
}

#pragma mark - Public

- (Browser*)browser {
  NOTREACHED_IN_MIGRATION();
  return nil;
}

- (Browser*)regularBrowser {
  // Ensure browser which is actually used by the regular coordinator is
  // returned, as it may have been updated.
  return _regularGridCoordinator ? _regularGridCoordinator.browser
                                 : _regularBrowser;
}

- (Browser*)incognitoBrowser {
  // Ensure browser which is actually used by the incognito coordinator is
  // returned, as it may have been updated.
  return _incognitoGridCoordinator ? _incognitoGridCoordinator.browser
                                   : _incognitoBrowser.get();
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  DCHECK(_incognitoGridCoordinator);
  [_incognitoGridCoordinator setIncognitoBrowser:incognitoBrowser];

  if (self.incognitoSnackbarCoordinator) {
    [self.incognitoSnackbarCoordinator stop];
    self.incognitoSnackbarCoordinator = nil;
  }

  if (incognitoBrowser) {
    self.incognitoSnackbarCoordinator = [[SnackbarCoordinator alloc]
        initWithBaseViewController:_baseViewController
                           browser:incognitoBrowser
                          delegate:self];
    [self.incognitoSnackbarCoordinator start];

    [incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:[self bookmarksCoordinator]
                     forProtocol:@protocol(BookmarksCommands)];
    [incognitoBrowser->GetCommandDispatcher()
        startDispatchingToTarget:self
                     forProtocol:@protocol(TabGridCommands)];
  }
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  // A modal may be presented on top of the Recent Tabs or tab grid.
  [self.baseViewController dismissModals];
  [self setActiveMode:TabGridMode::kNormal];

  [_incognitoGridCoordinator stopChildCoordinators];
  [_regularGridCoordinator stopChildCoordinators];
  if (IsTabGroupSyncEnabled()) {
    [_tabGroupsPanelCoordinator stopChildCoordinators];
  }

  [self dismissPopovers];

  [self.inactiveTabsCoordinator hide];

  if (_bookmarksCoordinator) {
    [_bookmarksCoordinator dismissBookmarkModalControllerAnimated:YES];
  }
  // History may be presented on top of the tab grid.
  if (self.historyCoordinator) {
    [self closeHistoryWithCompletion:completion];
  } else if (completion) {
    completion();
  }
}

- (void)setActiveMode:(TabGridMode)mode {
  _modeHolder.mode = mode;
}

- (UIViewController*)activeViewController {
  if (self.bvcContainer) {
    return self.bvcContainer.currentBVC;
  }
  return self.baseViewController;
}

- (BOOL)isTabGridActive {
  return self.bvcContainer == nil && !self.firstPresentation;
}

- (void)showTabGridPage:(TabGridPage)page {
  CHECK_NE(page, TabGridPageRemoteTabs);
  CHECK_NE(page, TabGridPageTabGroups);
  [_mediator setActivePage:page];

  BOOL animated = !self.animationsDisabledForTesting;

  SceneState* sceneState = self.regularBrowser->GetSceneState();
  [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:sceneState]
      logTabGridEntered];

  // Store the currentActivePage at this point in code, to be potentially used
  // during execution of the dispatched block to get the transition from Browser
  // to Tab Grid. That is because in some instances the active page might change
  // before the block gets executed, for example when closing the last tab in
  // incognito (crbug.com/1136882).
  TabGridPage currentActivePage = self.baseViewController.activePage;

  // Show "Bring Android Tabs" prompt if the user is an Android switcher and has
  // open tabs from their previous Android device.
  // Note: if the coordinator is already created, the prompt should have already
  // been displayed, therefore we should not need to display it again.
  BOOL shouldDisplayBringAndroidTabsPrompt = NO;
  if (currentActivePage == TabGridPageRegularTabs &&
      !_bringAndroidTabsPromptCoordinator) {
    BringAndroidTabsToIOSService* bringAndroidTabsService =
        BringAndroidTabsToIOSServiceFactory::GetForProfile(
            self.regularBrowser->GetProfile());
    if (bringAndroidTabsService != nil) {
      bringAndroidTabsService->LoadTabs();
      shouldDisplayBringAndroidTabsPrompt =
          bringAndroidTabsService->GetNumberOfAndroidTabs() > 0;
    }
  }

  // Determine the tab group, if any, of the active web state.
  const TabGroup* tabGroup = nullptr;
  WebStateList* webStateList = nullptr;
  if (currentActivePage == TabGridPageRegularTabs) {
    webStateList = self.regularBrowser->GetWebStateList();
  } else if (currentActivePage == TabGridPageIncognitoTabs) {
    webStateList = self.incognitoBrowser->GetWebStateList();
  }
  if (webStateList) {
    int activeWebStateIndex =
        webStateList->GetIndexOfWebState(webStateList->GetActiveWebState());
    if (webStateList->ContainsIndex(activeWebStateIndex)) {
      tabGroup = webStateList->GetGroupOfWebStateAt(activeWebStateIndex);
    }
  }

  BOOL toTabGroup = tabGroup != nullptr;

  __weak __typeof(self) weakSelf = self;

  ProceduralBlock transitionCompletionBlock = ^{
    [weakSelf transitionToGridCompleteForAndroidTabsPrompt:
                  shouldDisplayBringAndroidTabsPrompt];
  };

  ProceduralBlock transitionBlock = ^{
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    strongSelf.baseViewController.childViewControllerForStatusBarStyle = nil;

    if (IsNewTabGridTransitionsEnabled()) {
      [strongSelf
          performBrowserToTabGridTransitionWithAnimationEnabled:animated
                                                     completion:
                                                         transitionCompletionBlock];
    } else {
      [strongSelf
          performLegacyBrowserToTabGridTransitionWithActivePage:
              currentActivePage
                                               animationEnabled:animated
                                                     toTabGroup:toTabGroup
                                                     completion:
                                                         transitionCompletionBlock];
    }
  };

  // If a BVC is currently being presented, dismiss it.  This will trigger any
  // necessary animations.
  if (self.bvcContainer) {
    [self.baseViewController contentWillAppearAnimated:animated];
    // This is done with a dispatch to make sure that the view isn't added to
    // the view hierarchy right away, as it is not the expectations of the
    // API.
    dispatch_async(dispatch_get_main_queue(), transitionBlock);
  } else if (shouldDisplayBringAndroidTabsPrompt) {
    [self displayBringAndroidTabsPrompt];
  }

  if (tabGroup) {
    if (currentActivePage == TabGridPageRegularTabs) {
      [_regularGridCoordinator showTabGroupForTabGridOpening:tabGroup];
    } else if (currentActivePage == TabGridPageIncognitoTabs) {
      [_incognitoGridCoordinator showTabGroupForTabGridOpening:tabGroup];
    }
  }

  // Notify the Tab Groups panel (if any).
  [_tabGroupsPanelCoordinator prepareForAppearance];

  // Record when the tab switcher is presented.
  self.tabGridEnterTime = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("MobileTabGridEntered"));
  [self.priceCardMediator logMetrics:TAB_SWITCHER];
}

- (void)showTabViewController:(UIViewController*)viewController
                    incognito:(BOOL)incognito
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController || self.bvcContainer);

  __weak TabGridCoordinator* weakSelf = self;

  completion = ^{
    [weakSelf hideTabGroupsViews];
    if (completion) {
      completion();
    }
  };

  if (!self.tabGridEnterTime.is_null()) {
    // Record when the tab switcher is dismissed.
    base::RecordAction(base::UserMetricsAction("MobileTabGridExited"));

    // Record how long the tab switcher was presented.
    base::TimeDelta duration = base::TimeTicks::Now() - self.tabGridEnterTime;
    base::UmaHistogramLongTimes("IOS.TabSwitcher.TimeSpent", duration);
    self.tabGridEnterTime = base::TimeTicks();
  }

  SceneState* sceneState = self.regularBrowser->GetSceneState();
  sceneState.window.overrideUserInterfaceStyle =
      UIUserInterfaceStyleUnspecified;

  // If another BVC is already being presented, swap this one into the
  // container.
  if (self.bvcContainer) {
    self.bvcContainer.currentBVC = viewController;
    self.bvcContainer.incognito = incognito;
    self.baseViewController.childViewControllerForStatusBarStyle =
        viewController;
    [self.baseViewController setNeedsStatusBarAppearanceUpdate];
    if (completion) {
      completion();
    }
    return;
  }

  self.bvcContainer = [[BVCContainerViewController alloc] init];
  self.bvcContainer.currentBVC = viewController;
  self.bvcContainer.incognito = incognito;
  // Set fallback presenter, because currentBVC can be nil if the tab grid is
  // up but no tabs exist in current page.
  self.bvcContainer.fallbackPresenterViewController = self.baseViewController;

  BOOL animated = !self.animationsDisabledForTesting;
  // Never animate the first time.
  if (self.firstPresentation)
    animated = NO;

  // Extend `completion` to signal the tab switcher delegate
  // that the animated "tab switcher dismissal" (that is, presenting something
  // on top of the tab switcher) transition has completed.
  // Finally, the launch mask view should be removed.
  ProceduralBlock extendedCompletion = ^{
    [self.delegate tabGridDismissTransitionDidEnd:self];
    // In search mode, the tabgrid mode is not reset before the animation so
    // the animation can start from the correct cell. Once the animation is
    // complete, reset the tab grid mode.
    [self setActiveMode:TabGridMode::kNormal];
    Browser* browser = self.bvcContainer.incognito ? self.incognitoBrowser
                                                   : self.regularBrowser;
    if (!GetFirstResponderInWindowScene(
            self.baseViewController.view.window.windowScene) &&
        !FindNavigatorShouldBePresentedInBrowser(browser)) {
      // It is possible to already have a first responder (for example the
      // omnibox). In that case, we don't want to mark BVC as first responder.
      [self.bvcContainer.currentBVC becomeFirstResponder];
    }
    if (completion) {
      completion();
    }
    self.firstPresentation = NO;
    [weakSelf hideTabGroupsViews];
  };

  self.baseViewController.childViewControllerForStatusBarStyle =
      self.bvcContainer.currentBVC;

  [self.baseViewController contentWillDisappearAnimated:animated];

  if (IsNewTabGridTransitionsEnabled()) {
    [self performTabGridToBrowserTransitionWithAnimationEnabled:animated
                                                     completion:
                                                         extendedCompletion];
  } else {
    [self performLegacyTabGridToBrowserTransitionWithActivePage:
              self.baseViewController.activePage
                                               animationEnabled:animated
                                                     completion:
                                                         extendedCompletion];
  }
}

#pragma mark - Private

// Hides tab group views.
- (void)hideTabGroupsViews {
  [_incognitoGridCoordinator hideTabGroup];
  [_regularGridCoordinator hideTabGroup];
}

// Lazily creates the bookmarks coordinator.
- (BookmarksCoordinator*)bookmarksCoordinator {
  if (!_bookmarksCoordinator) {
    _bookmarksCoordinator =
        [[BookmarksCoordinator alloc] initWithBrowser:self.regularBrowser];
    _bookmarksCoordinator.baseViewController = self.baseViewController;
  }
  return _bookmarksCoordinator;
}

- (void)displayBringAndroidTabsPrompt {
  if (!_bringAndroidTabsPromptCoordinator) {
    _bringAndroidTabsPromptCoordinator =
        [[BringAndroidTabsPromptCoordinator alloc]
            initWithBaseViewController:self.baseViewController
                               browser:self.regularBrowser];
    _bringAndroidTabsPromptCoordinator.commandHandler = self;
  }
  [_bringAndroidTabsPromptCoordinator start];
  [self.baseViewController
      presentViewController:_bringAndroidTabsPromptCoordinator.viewController
                   animated:YES
                 completion:nil];
}

// Performs the new Browser to Tab Grid transition.
- (void)performBrowserToTabGridTransitionWithAnimationEnabled:
            (BOOL)animationEnabled
                                                   completion:
                                                       (ProceduralBlock)
                                                           completionHandler {
  TabGridTransitionDirection direction =
      TabGridTransitionDirection::kFromBrowserToTabGrid;
  TabGridTransitionType transitionType = [self
      determineTabGridTransitionTypeWithAnimationEnabled:animationEnabled];

  self.transitionHandler = [[TabGridTransitionHandler alloc]
          initWithTransitionType:transitionType
                       direction:direction
           tabGridViewController:self.baseViewController
      bvcContainerViewController:self.bvcContainer];
  [self.transitionHandler performTransitionWithCompletion:completionHandler];
}

// Performs the new Tab Grid to Browser transition.
- (void)performTabGridToBrowserTransitionWithAnimationEnabled:
            (BOOL)animationEnabled
                                                   completion:
                                                       (ProceduralBlock)
                                                           completionHandler {
  TabGridTransitionDirection direction =
      TabGridTransitionDirection::kFromTabGridToBrowser;
  TabGridTransitionType transitionType = [self
      determineTabGridTransitionTypeWithAnimationEnabled:animationEnabled];

  self.transitionHandler = [[TabGridTransitionHandler alloc]
          initWithTransitionType:transitionType
                       direction:direction
           tabGridViewController:self.baseViewController
      bvcContainerViewController:self.bvcContainer];
  [self.transitionHandler performTransitionWithCompletion:completionHandler];
}

// Performs the legacy Browser to Tab Grid transition, `toTabGroup` or not.
- (void)
    performLegacyBrowserToTabGridTransitionWithActivePage:
        (TabGridPage)activePage
                                         animationEnabled:(BOOL)animationEnabled
                                               toTabGroup:(BOOL)toTabGroup
                                               completion:
                                                   (ProceduralBlock)completion {
  if (!self.bvcContainer) {
    // It is possible that the Grid is presented twice in a row. Because the
    // detection of "the Browser is visible" is based on a null check of
    // `self.bvcContainer` which is nullified at the end of the animation, so
    // two animations could be started in a short sequence.
    return;
  }
  self.legacyTransitionHandler =
      [self createTransitionHanlderWithAnimationEnabled:animationEnabled];
  [self.legacyTransitionHandler transitionFromBrowser:self.bvcContainer
                                            toTabGrid:self.baseViewController
                                           toTabGroup:toTabGroup
                                           activePage:activePage
                                       withCompletion:completion];
}

// Performs the legacy Tab Grid to Browser transition.
- (void)
    performLegacyTabGridToBrowserTransitionWithActivePage:
        (TabGridPage)activePage
                                         animationEnabled:(BOOL)animationEnabled
                                               completion:
                                                   (ProceduralBlock)completion {
  self.legacyTransitionHandler =
      [self createTransitionHanlderWithAnimationEnabled:animationEnabled];
  [self.legacyTransitionHandler transitionFromTabGrid:self.baseViewController
                                            toBrowser:self.bvcContainer
                                           activePage:activePage
                                       withCompletion:completion];
}

// Called when the transition from Browser to Tab Grid is complete and whether
// it `shouldDisplayBringAndroidTabsPrompt`.
- (void)transitionToGridCompleteForAndroidTabsPrompt:
    (BOOL)shouldDisplayBringAndroidTabsPrompt {
  self.bvcContainer = nil;
  _frameWhenEntering = self.baseViewController.view.frame;
  [self.baseViewController contentDidAppear];

  if (shouldDisplayBringAndroidTabsPrompt) {
    [self displayBringAndroidTabsPrompt];
  }

  UIWindow* sceneWindow = self.regularBrowser->GetSceneState().window;
  sceneWindow.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
}

// Creates a transition handler with `animationEnabled` parameter.
- (LegacyTabGridTransitionHandler*)createTransitionHanlderWithAnimationEnabled:
    (BOOL)animationEnabled {
  LegacyTabGridTransitionHandler* transitionHandler =
      [[LegacyTabGridTransitionHandler alloc] initWithLayoutProvider:self];
  transitionHandler.animationDisabled = !animationEnabled;

  return transitionHandler;
}

// Determines the transion type to be used in the transition.
- (TabGridTransitionType)determineTabGridTransitionTypeWithAnimationEnabled:
    (BOOL)animationEnabled {
  if (!animationEnabled) {
    return TabGridTransitionType::kAnimationDisabled;
  } else if (UIAccessibilityIsReduceMotionEnabled()) {
    return TabGridTransitionType::kReducedMotion;
  }

  return TabGridTransitionType::kNormal;
}

// YES if there are tabs present on `page`. Should be called for regular or
// incognito.
- (BOOL)tabsPresentForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageRegularTabs:
      return !self.regularBrowser->GetWebStateList()->empty();
    case TabGridPageIncognitoTabs:
      return !self.incognitoBrowser->GetWebStateList()->empty();
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      NOTREACHED_NORETURN();
  }
}

#pragma mark - ChromeCoordinator

- (void)start {
  _modeHolder = [[TabGridModeHolder alloc] init];

  [_regularBrowser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabGridCommands)];
  [_incognitoBrowser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(TabGridCommands)];

  ProfileIOS* profile = self.regularBrowser->GetProfile();
  _mediator = [[TabGridMediator alloc]
       initWithIdentityManager:IdentityManagerFactory::GetForProfile(profile)
                   prefService:profile->GetPrefs()
      featureEngagementTracker:feature_engagement::TrackerFactory::
                                   GetForProfile(profile)
                    modeHolder:_modeHolder];

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);

  TabGridViewController* baseViewController = [[TabGridViewController alloc]
      initWithPageConfiguration:_pageConfiguration];
  baseViewController.handler = applicationCommandsHandler;
  baseViewController.tabPresentationDelegate = self;
  baseViewController.layoutGuideCenter = LayoutGuideCenterForBrowser(nil);
  baseViewController.delegate = self;
  baseViewController.tabGridHandler = self;
  baseViewController.mutator = _mediator;
  _baseViewController = baseViewController;

  _mediator.consumer = _baseViewController;

  _toolbarsCoordinator = [[TabGridToolbarsCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:_regularBrowser];
  _toolbarsCoordinator.searchDelegate = self.baseViewController;
  _toolbarsCoordinator.toolbarTabGridDelegate = self.baseViewController;
  _toolbarsCoordinator.modeHolder = _modeHolder;
  [_toolbarsCoordinator start];
  self.baseViewController.topToolbar = _toolbarsCoordinator.topToolbar;
  self.baseViewController.bottomToolbar = _toolbarsCoordinator.bottomToolbar;

  _regularGridCoordinator = [[RegularGridCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_regularBrowser
                 toolbarsMutator:_toolbarsCoordinator.toolbarsMutator
            gridMediatorDelegate:self];
  _regularGridCoordinator.disabledTabViewControllerDelegate =
      self.baseViewController;
  _regularGridCoordinator.tabGroupPositioner = self;
  _regularGridCoordinator.tabContextMenuDelegate = self;
  _regularGridCoordinator.modeHolder = _modeHolder;

  [_regularGridCoordinator start];

  baseViewController.regularTabsViewController =
      _regularGridCoordinator.gridViewController;
  baseViewController.regularDisabledGridViewController =
      _regularGridCoordinator.disabledViewController;
  baseViewController.regularGridContainerViewController =
      _regularGridCoordinator.gridContainerViewController;
  baseViewController.pinnedTabsViewController =
      _regularGridCoordinator.pinnedTabsViewController;
  baseViewController.regularGridHandler = _regularGridCoordinator.gridHandler;
  self.regularTabsMediator = _regularGridCoordinator.regularGridMediator;

  ProfileIOS* regularProfile =
      _regularBrowser ? _regularBrowser->GetProfile() : nullptr;
  WebStateList* regularWebStateList =
      _regularBrowser ? _regularBrowser->GetWebStateList() : nullptr;
  self.priceCardMediator =
      [[PriceCardMediator alloc] initWithWebStateList:regularWebStateList];

  // Offer to manage inactive regular tabs iff the regular tabs grid is
  // available. The regular tabs can be disabled by policy, making the grid
  // unavailable.
  if (IsInactiveTabsAvailable() &&
      _pageConfiguration != TabGridPageConfiguration::kIncognitoPageOnly) {
    CHECK(_regularGridCoordinator.gridViewController);
    self.inactiveTabsButtonMediator = [[InactiveTabsButtonMediator alloc]
        initWithConsumer:_regularGridCoordinator.gridViewController
            webStateList:_inactiveBrowser->GetWebStateList()
             prefService:GetApplicationContext()->GetLocalState()];
  }

  baseViewController.priceCardDataSource = self.priceCardMediator;


  _incognitoGridCoordinator = [[IncognitoGridCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_incognitoBrowser
                 toolbarsMutator:_toolbarsCoordinator.toolbarsMutator
            gridMediatorDelegate:self];
  _incognitoGridCoordinator.disabledTabViewControllerDelegate =
      self.baseViewController;
  _incognitoGridCoordinator.tabGroupPositioner = self;
  _incognitoGridCoordinator.audience = self;
  _incognitoGridCoordinator.tabContextMenuDelegate = self;
  _incognitoGridCoordinator.modeHolder = _modeHolder;

  [_incognitoGridCoordinator start];

  self.incognitoTabsMediator = _incognitoGridCoordinator.incognitoGridMediator;
  [self.incognitoTabsMediator
      initializeSupervisedUserCapabilitiesObserver:IdentityManagerFactory::
                                                       GetForProfile(profile)];

  baseViewController.incognitoGridHandler =
      _incognitoGridCoordinator.gridHandler;

  baseViewController.incognitoTabsViewController =
      _incognitoGridCoordinator.gridViewController;
  baseViewController.incognitoDisabledGridViewController =
      _incognitoGridCoordinator.disabledViewController;
  baseViewController.incognitoGridContainerViewController =
      _incognitoGridCoordinator.gridContainerViewController;

  self.recentTabsContextMenuHelper =
      [[RecentTabsContextMenuHelper alloc] initWithBrowser:self.regularBrowser
                            recentTabsPresentationDelegate:self
                                    tabContextMenuDelegate:self];
  self.baseViewController.remoteTabsViewController.menuProvider =
      self.recentTabsContextMenuHelper;

  if (IsTabGroupSyncEnabled()) {
    _tabGroupsPanelCoordinator = [[TabGroupsPanelCoordinator alloc]
            initWithBaseViewController:_baseViewController
                        regularBrowser:_regularBrowser
                       toolbarsMutator:_toolbarsCoordinator.toolbarsMutator
        disabledViewControllerDelegate:_baseViewController];

    [_tabGroupsPanelCoordinator start];

    baseViewController.tabGroupsPanelViewController =
        _tabGroupsPanelCoordinator.gridViewController;
    baseViewController.tabGroupsDisabledGridViewController =
        _tabGroupsPanelCoordinator.disabledViewController;
    baseViewController.tabGroupsGridContainerViewController =
        _tabGroupsPanelCoordinator.gridContainerViewController;

    // TODO(crbug.com/346302283): Some tests depend on a
    // RecentTabsTableViewController to have been loaded and kept in memory.
    // Investigate and remove this dependency.
    RecentTabsTableViewController* remoteTabsViewController =
        [[RecentTabsTableViewController alloc] init];
    remoteTabsViewController.browser = self.regularBrowser;
    [remoteTabsViewController loadModel];
    [remoteTabsViewController.tableView reloadData];
    _hackRecentTabsTableViewController = remoteTabsViewController;
  } else {
    // TODO(crbug.com/41390276) : Remove RecentTabsTableViewController
    // dependency on ProfileIOS so that we don't need to expose the view
    // controller.
    baseViewController.remoteTabsViewController.browser = self.regularBrowser;
    sync_sessions::SessionSyncService* syncService =
        SessionSyncServiceFactory::GetForProfile(regularProfile);
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(regularProfile);
    sessions::TabRestoreService* restoreService =
        IOSChromeTabRestoreServiceFactory::GetForProfile(regularProfile);
    FaviconLoader* faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForProfile(regularProfile);
    syncer::SyncService* service =
        SyncServiceFactory::GetForProfile(regularProfile);
    BrowserList* browserList =
        BrowserListFactory::GetForProfile(regularProfile);
    SceneState* currentSceneState = self.regularBrowser->GetSceneState();
    // TODO(crbug.com/40273478): Rename in recentTabsMediator.
    self.remoteTabsMediator = [[RecentTabsMediator alloc]
        initWithSessionSyncService:syncService
                   identityManager:identityManager
                    restoreService:restoreService
                     faviconLoader:faviconLoader
                       syncService:service
                       browserList:browserList
                        sceneState:currentSceneState
                  disabledByPolicy:_pageConfiguration ==
                                   TabGridPageConfiguration::kIncognitoPageOnly
                 engagementTracker:feature_engagement::TrackerFactory::
                                       GetForProfile(regularProfile)
                        modeHolder:_modeHolder];
    self.remoteTabsMediator.consumer = baseViewController.remoteTabsConsumer;
    self.remoteTabsMediator.tabGridHandler = self;
    baseViewController.remoteTabsViewController.imageDataSource =
        self.remoteTabsMediator;
    baseViewController.remoteTabsViewController.delegate =
        self.remoteTabsMediator;
    baseViewController.remoteTabsViewController.applicationHandler =
        applicationCommandsHandler;
    baseViewController.remoteTabsViewController.loadStrategy =
        UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
    baseViewController.remoteTabsViewController.presentationDelegate = self;
    baseViewController.activityObserver = self.remoteTabsMediator;

    _remoteGridContainerViewController =
        [[GridContainerViewController alloc] init];
    self.baseViewController.remoteGridContainerViewController =
        _remoteGridContainerViewController;
  }

  if (IsInactiveTabsAvailable()) {
    self.inactiveTabsCoordinator = [[InactiveTabsCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:_inactiveBrowser
                          delegate:self];
    self.inactiveTabsCoordinator.tabContextMenuDelegate = self;

    [self.inactiveTabsCoordinator start];

    self.regularTabsMediator.containedGridToolbarsProvider =
        self.inactiveTabsCoordinator.toolbarsConfigurationProvider;
    self.regularTabsMediator.inactiveTabsGridCommands =
        self.inactiveTabsCoordinator.gridCommandsHandler;
  }

  self.firstPresentation = YES;

  // TODO(crbug.com/41393201) : Currently, consumer calls from the mediator
  // prematurely loads the view in `RecentTabsTableViewController`. Fix this so
  // that the view is loaded only by an explicit placement in the view
  // hierarchy. As a workaround, the view controller hierarchy is loaded here
  // before `RecentTabsMediator` updates are started.
  self.window.rootViewController = self.baseViewController;
  if (regularProfile) {
    [self.remoteTabsMediator initObservers];
    [self.remoteTabsMediator refreshSessionsView];
  }

  _mediator.regularPageMutator = _regularGridCoordinator.regularGridMediator;
  _mediator.incognitoPageMutator = self.incognitoTabsMediator;
  if (IsTabGroupSyncEnabled()) {
    _mediator.tabGroupsPageMutator = _tabGroupsPanelCoordinator.mediator;
  } else {
    _mediator.remotePageMutator = self.remoteTabsMediator;
  }
  _mediator.toolbarsMutator = _toolbarsCoordinator.toolbarsMutator;

  self.remoteTabsMediator.toolbarsMutator =
      _toolbarsCoordinator.toolbarsMutator;

  self.incognitoTabsMediator.tabPresentationDelegate = self;
  self.regularTabsMediator.tabPresentationDelegate = self;

  self.incognitoTabsMediator.gridConsumer = self.baseViewController;
  self.regularTabsMediator.gridConsumer = self.baseViewController;
  self.remoteTabsMediator.gridConsumer = self.baseViewController;

  self.incognitoTabsMediator.tabGridIdleStatusHandler = self.baseViewController;
  self.regularTabsMediator.tabGridIdleStatusHandler = self.baseViewController;

  self.snackbarCoordinator =
      [[SnackbarCoordinator alloc] initWithBaseViewController:baseViewController
                                                      browser:_regularBrowser
                                                     delegate:self];
  [self.snackbarCoordinator start];
  self.incognitoSnackbarCoordinator =
      [[SnackbarCoordinator alloc] initWithBaseViewController:baseViewController
                                                      browser:_incognitoBrowser
                                                     delegate:self];
  [self.incognitoSnackbarCoordinator start];

  [_regularBrowser->GetCommandDispatcher()
      startDispatchingToTarget:[self bookmarksCoordinator]
                   forProtocol:@protocol(BookmarksCommands)];
  [_incognitoBrowser->GetCommandDispatcher()
      startDispatchingToTarget:[self bookmarksCoordinator]
                   forProtocol:@protocol(BookmarksCommands)];

  SceneState* sceneState = self.regularBrowser->GetSceneState();
  [sceneState addObserver:self];

  // Once the mediators are set up, stop keeping pointers to the browsers used
  // to initialize them.
  _regularBrowser = nil;
  _incognitoBrowser = nil;
}

- (void)stop {
  SceneState* sceneState = self.regularBrowser->GetSceneState();
  [sceneState removeObserver:self];

  // The TabGridViewController may still message its application commands
  // handler after this coordinator has stopped; make this action a no-op by
  // setting the handler to nil.
  self.baseViewController.handler = nil;
  self.recentTabsContextMenuHelper = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  [self.incognitoBrowser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self.regularBrowser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(SettingsCommands)];

  [_toolbarsCoordinator stop];
  _toolbarsCoordinator = nil;

  [_incognitoGridCoordinator stop];
  _incognitoGridCoordinator = nil;

  [_regularGridCoordinator stop];
  _regularGridCoordinator = nil;

  [_tabGroupsPanelCoordinator stop];
  _tabGroupsPanelCoordinator = nil;

  if (IsTabGroupSyncEnabled()) {
    // This disconnects the Recent Tabs' SigninPromoViewMediator.
    [_hackRecentTabsTableViewController dismissModals];
  }

  // TODO(crbug.com/41390276) : RecentTabsTableViewController behaves like a
  // coordinator and that should be factored out.
  [self.baseViewController.remoteTabsViewController dismissModals];
  self.baseViewController.remoteTabsViewController.browser = nil;
  [self.remoteTabsMediator disconnect];
  self.remoteTabsMediator = nil;
  [self dismissActionSheetCoordinator];

  [self.snackbarCoordinator stop];
  self.snackbarCoordinator = nil;
  [self.incognitoSnackbarCoordinator stop];
  self.incognitoSnackbarCoordinator = nil;

  [_bringAndroidTabsPromptCoordinator stop];
  _bringAndroidTabsPromptCoordinator = nil;
  [_tabListFromAndroidCoordinator stop];
  _tabListFromAndroidCoordinator = nil;

  [self.inactiveTabsButtonMediator disconnect];
  self.inactiveTabsButtonMediator = nil;
  [self.inactiveTabsCoordinator stop];
  self.inactiveTabsCoordinator = nil;

  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;

  [_bookmarksCoordinator stop];
  _bookmarksCoordinator = nil;

  [_mediator disconnect];
}

#pragma mark - TabPresentationDelegate

- (void)showActiveTabInPage:(TabGridPage)page focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(self.regularBrowser && self.incognitoBrowser);
  [_mediator setActivePage:page];

  Browser* activeBrowser = nullptr;
  switch (page) {
    case TabGridPageIncognitoTabs:
      DCHECK_GT(self.incognitoBrowser->GetWebStateList()->count(), 0);
      activeBrowser = self.incognitoBrowser;
      break;
    case TabGridPageRegularTabs:
      DCHECK_GT(self.regularBrowser->GetWebStateList()->count(), 0);
      activeBrowser = self.regularBrowser;
      break;
    case TabGridPageRemoteTabs:
      DUMP_WILL_BE_NOTREACHED()
          << "It is invalid to have an active tab in Recent Tabs.";
      // This appears to come up in release -- see crbug.com/1069243.
      // Defensively early return instead of continuing.
      return;
    case TabGridPageTabGroups:
      DUMP_WILL_BE_NOTREACHED()
          << "It is invalid to have an active tab in Tab Groups.";
      // This may come up in release -- see crbug.com/1069243.
      // Defensively early return instead of continuing.
      return;
  }
  // Trigger the transition through the delegate. This will in turn call back
  // into this coordinator.
  [self.delegate tabGrid:self
      shouldActivateBrowser:activeBrowser
               focusOmnibox:focusOmnibox];
}

#pragma mark - GridMediatorDelegate

- (void)baseGridMediator:(BaseGridMediator*)baseGridMediator
    showCloseConfirmationWithTabIDs:(const std::set<web::WebStateID>&)tabIDs
                           groupIDs:
                               (const std::set<tab_groups::TabGroupId>&)groupIDs
                           tabCount:(int)tabCount
                             anchor:(UIBarButtonItem*)buttonAnchor {
  if (baseGridMediator == self.regularTabsMediator) {
    base::RecordAction(base::UserMetricsAction(
        "MobileTabGridSelectionCloseRegularTabsConfirmationPresented"));

    self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.regularBrowser
                             title:nil
                           message:nil
                     barButtonItem:buttonAnchor];

    // IOS 17 Bug: The alert arrow direction presentation is broken.
    // Workaround: Specifically set the popover arrow direction. (crbug/1490535)
    if (@available(iOS 17, *)) {
      self.actionSheetCoordinator.popoverArrowDirection =
          UIPopoverArrowDirectionDown | UIPopoverArrowDirectionUp;
    }
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileTabGridSelectionCloseIncognitoTabsConfirmationPresented"));

    self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.incognitoBrowser
                             title:nil
                           message:nil
                     barButtonItem:buttonAnchor];
  }

  self.actionSheetCoordinator.alertStyle = UIAlertControllerStyleActionSheet;

  __weak BaseGridMediator* weakBaseGridMediator = baseGridMediator;
  __weak TabGridCoordinator* weakSelf = self;
  // Copy the set of tab and group identifiers, so that the following block can
  // use it.
  std::set<web::WebStateID> tabIDsCopy = tabIDs;
  std::set<tab_groups::TabGroupId> groupIDsCopy = groupIDs;

  [self.actionSheetCoordinator
      addItemWithTitle:base::SysUTF16ToNSString(
                           l10n_util::GetPluralStringFUTF16(
                               IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
                               tabCount))
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileTabGridSelectionCloseTabsConfirmed"));
                  [weakBaseGridMediator closeItemsWithTabIDs:tabIDsCopy
                                                    groupIDs:groupIDsCopy
                                                    tabCount:tabCount];
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleDestructive];
  [self.actionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileTabGridSelectionCloseTabsCanceled"));
                  [weakSelf dismissActionSheetCoordinator];
                }
                 style:UIAlertActionStyleCancel];
  [self.actionSheetCoordinator start];
}

- (void)baseGridMediator:(BaseGridMediator*)baseGridMediator
               shareURLs:(NSArray<URLWithTitle*>*)URLs
                  anchor:(UIBarButtonItem*)buttonAnchor {
  SharingParams* params = [[SharingParams alloc]
      initWithURLs:URLs
          scenario:SharingScenario::TabGridSelectionMode];

  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.regularBrowser
                          params:params
                          anchor:buttonAnchor];
  [self.sharingCoordinator start];
}

- (void)dismissActionSheetCoordinator {
  [self.actionSheetCoordinator stop];
  self.actionSheetCoordinator = nil;
}

- (void)dismissPopovers {
  [self dismissActionSheetCoordinator];
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
}

#pragma mark - GridCoordinatorAudience

- (void)incognitoGridDidChange {
  self.baseViewController.incognitoTabsViewController =
      _incognitoGridCoordinator.gridViewController;
}

#pragma mark - TabGridViewControllerDelegate

- (void)openLinkWithURL:(const GURL&)URL {
  id<ApplicationCommands> handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  [handler openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:URL]];
}

- (void)showInactiveTabs {
  CHECK(IsInactiveTabsEnabled());
  [self.inactiveTabsCoordinator show];
}

- (BOOL)tabGridIsUserEligibleForSwipeToIncognitoIPH {
  return _pageConfiguration == TabGridPageConfiguration::kAllPagesEnabled &&
         IsFirstRunRecent(base::Days(60)) &&
         feature_engagement::TrackerFactory::GetForProfile(
             self.regularBrowser->GetProfile())
             ->WouldTriggerHelpUI(
                 feature_engagement::kIPHiOSTabGridSwipeRightForIncognito);
}

- (BOOL)tabGridShouldPresentSwipeToIncognitoIPH {
  return feature_engagement::TrackerFactory::GetForProfile(
             self.regularBrowser->GetProfile())
      ->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSTabGridSwipeRightForIncognito);
}

- (void)tabGridDidDismissSwipeToIncognitoIPHWithReason:
    (IPHDismissalReasonType)reason {
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.regularBrowser->GetProfile());
  if (tracker) {
    tracker->DismissedWithSnooze(
        feature_engagement::kIPHiOSTabGridSwipeRightForIncognito,
        feature_engagement::Tracker::SnoozeAction::DISMISSED);
    if (reason == IPHDismissalReasonType::kTappedClose) {
      tracker->NotifyEvent(
          feature_engagement::events::
              kIOSSwipeRightForIncognitoIPHDismissButtonTapped);
    }
  }
}

#pragma mark - InactiveTabsCoordinatorDelegate

- (void)inactiveTabsCoordinator:
            (InactiveTabsCoordinator*)inactiveTabsCoordinator
            didSelectItemWithID:(web::WebStateID)itemID {
  WebStateList* regularWebStateList = self.regularBrowser->GetWebStateList();
  int toInsertIndex = regularWebStateList->count();

  MoveTabToBrowser(itemID, self.regularBrowser, toInsertIndex);

  // TODO(crbug.com/40896001): Adapt the animation so the grid animation is
  // coming from the inactive panel.
  regularWebStateList->ActivateWebStateAt(toInsertIndex);
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.regularBrowser
               focusOmnibox:NO];
}

- (void)inactiveTabsCoordinatorDidFinish:
    (InactiveTabsCoordinator*)inactiveTabsCoordinator {
  CHECK(IsInactiveTabsAvailable());
  [self.inactiveTabsCoordinator hide];
}

#pragma mark - RecentTabsPresentationDelegate

- (void)showHistoryFromRecentTabsFilteredBySearchTerms:(NSString*)searchTerms {
  [self showHistoryForText:searchTerms];
}

- (void)showActiveRegularTabFromRecentTabs {
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.regularBrowser
               focusOmnibox:NO];
}

- (void)showRegularTabGridFromRecentTabs {
  [self.baseViewController setCurrentPageAndPageControl:TabGridPageRegularTabs
                                               animated:YES];
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
      initWithBaseViewController:_baseViewController
                         browser:self.regularBrowser
                   showUserEmail:!dedicatedSignInDone
               signOutIfDeclined:dedicatedSignInDone
                      isOptional:NO
                     accessPoint:signin_metrics::AccessPoint::
                                     ACCESS_POINT_RECENT_TABS];
  _historySyncPopupCoordinator.delegate = self;
  [_historySyncPopupCoordinator start];
}

#pragma mark - HistoryPresentationDelegate

- (void)showActiveRegularTabFromHistory {
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.regularBrowser
               focusOmnibox:NO];
}

- (void)showActiveIncognitoTabFromHistory {
  [self.delegate tabGrid:self
      shouldActivateBrowser:self.incognitoBrowser
               focusOmnibox:NO];
}

- (void)openAllTabsFromSession:(const synced_sessions::DistantSession*)session {
  base::RecordAction(base::UserMetricsAction(
      "MobileRecentTabManagerOpenAllTabsFromOtherDevice"));
  base::UmaHistogramCounts100(
      "Mobile.RecentTabsManager.TotalTabsFromOtherDevicesOpenAll",
      session->tabs.size());

  BOOL inIncognito = self.regularBrowser->GetProfile()->IsOffTheRecord();
  OpenDistantSessionInBackground(
      session, inIncognito, GetDefaultNumberOfTabsToLoadSimultaneously(),
      UrlLoadingBrowserAgent::FromBrowser(self.regularBrowser),
      self.baseViewController.remoteTabsViewController.loadStrategy);

  [self showActiveRegularTabFromRecentTabs];
}

#pragma mark - HistoryCoordinatorDelegate

- (void)closeHistoryWithCompletion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  [self.historyCoordinator dismissWithCompletion:^{
    if (completion) {
      completion();
    }
    [weakSelf.historyCoordinator stop];
    weakSelf.historyCoordinator = nil;
  }];
}

- (void)closeHistory {
  [self closeHistoryWithCompletion:nil];
}

#pragma mark - TabContextMenuDelegate

- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        scenario:(SharingScenario)scenario
        fromView:(UIView*)view {
  SharingParams* params = [[SharingParams alloc] initWithURL:URL
                                                       title:title
                                                    scenario:scenario];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.regularBrowser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

- (void)addToReadingListURL:(const GURL&)URL title:(NSString*)title {
  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURL:URL title:title];
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.regularBrowser);
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

- (void)bookmarkURL:(const GURL&)URL title:(NSString*)title {
  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForProfile(
          self.regularBrowser->GetProfile());
  if (bookmarkModel->IsBookmarked(URL)) {
    [self editBookmarkWithURL:URL];
  } else {
    base::RecordAction(base::UserMetricsAction(
        "MobileTabGridOpenedBookmarkEditorForNewBookmark"));
    [self.bookmarksCoordinator createBookmarkURL:URL title:title];
  }
}

- (void)editBookmarkWithURL:(const GURL&)URL {
  base::RecordAction(base::UserMetricsAction(
      "MobileTabGridOpenedBookmarkEditorForExistingBookmark"));
  [self.bookmarksCoordinator presentBookmarkEditorForURL:URL];
}

- (void)pinTabWithIdentifier:(web::WebStateID)identifier {
  [self.regularTabsMediator setPinState:YES forItemWithID:identifier];
}

- (void)unpinTabWithIdentifier:(web::WebStateID)identifier {
  [self.regularTabsMediator setPinState:NO forItemWithID:identifier];
}

- (void)createNewTabGroupWithIdentifier:(web::WebStateID)identifier
                              incognito:(BOOL)incognito {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a new tab group outside the Tab "
         "Groups experiment.";
  std::set<web::WebStateID> webStateIDSet = {identifier};
  if (incognito) {
    [_incognitoGridCoordinator showTabGroupCreationForTabs:webStateIDSet];
  } else {
    [_regularGridCoordinator showTabGroupCreationForTabs:webStateIDSet];
  }
}

- (void)editTabGroup:(base::WeakPtr<const TabGroup>)group
           incognito:(BOOL)incognito {
  if (!group) {
    return;
  }
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to edit a tab group outside the Tab Groups "
         "experiment.";

  BaseGridCoordinator* coordinator;
  if (incognito) {
    coordinator = _incognitoGridCoordinator;
  } else {
    coordinator = _regularGridCoordinator;
  }
  [coordinator showTabGroupEditionForGroup:group.get()];
}

- (void)closeTabWithIdentifier:(web::WebStateID)identifier
                     incognito:(BOOL)incognito {
  if (incognito) {
    [self.incognitoTabsMediator closeItemWithID:identifier];
    return;
  }

  [self.regularTabsMediator closeItemWithID:identifier];
}

- (void)deleteTabGroup:(base::WeakPtr<const TabGroup>)group
             incognito:(BOOL)incognito
            sourceView:(UIView*)sourceView {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to delete a tab group outside the Tab Groups "
         "experiment.";
  if (incognito) {
    CHECK(!IsTabGroupSyncEnabled());
    [self.incognitoTabsMediator deleteTabGroup:group sourceView:sourceView];
    return;
  }

  [self.regularTabsMediator deleteTabGroup:group sourceView:sourceView];
}

- (void)closeTabGroup:(base::WeakPtr<const TabGroup>)group
            incognito:(BOOL)incognito {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to close a tab group outside the Tab Groups "
         "experiment.";
  if (incognito) {
    [self.incognitoTabsMediator closeTabGroup:group];
    return;
  }

  [self.regularTabsMediator closeTabGroup:group];
}

- (void)ungroupTabGroup:(base::WeakPtr<const TabGroup>)group
              incognito:(BOOL)incognito
             sourceView:(UIView*)sourceView {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to ungroup a tab group outside the Tab Groups "
         "experiment.";
  if (incognito) {
    [self.incognitoTabsMediator ungroupTabGroup:group sourceView:sourceView];
    return;
  }

  [self.regularTabsMediator ungroupTabGroup:group sourceView:sourceView];
}

- (void)selectTabs {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridTabContextMenuSelectTabs"));
  [self setActiveMode:TabGridMode::kSelection];
}

- (void)removeSessionAtTableSectionWithIdentifier:(NSInteger)sectionIdentifier {
  [self.baseViewController.remoteTabsViewController
      removeSessionAtTableSectionWithIdentifier:sectionIdentifier];
}

- (synced_sessions::DistantSession const*)sessionForTableSectionWithIdentifier:
    (NSInteger)sectionIdentifier {
  return [self.baseViewController.remoteTabsViewController
      sessionForTableSectionWithIdentifier:sectionIdentifier];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelBackground) {
    // When going in the background, hide the Inactive Tabs UI.
    [self.inactiveTabsCoordinator hide];
  }
  if (level < SceneActivationLevelForegroundActive) {
    // User has put the app into background, which constitutes of a meaningful
    // action.
    [self.baseViewController
        tabGridDidPerformAction:TabGridActionType::kBackground];
  }
}

#pragma mark - BringAndroidTabsCommands

- (void)reviewAllBringAndroidTabs {
  [self onUserInteractionWithBringAndroidTabsPrompt:YES];
}

- (void)dismissBringAndroidTabsPrompt {
  [self onUserInteractionWithBringAndroidTabsPrompt:NO];
}

// Helper method to handle BringAndroidTabsCommands.
- (void)onUserInteractionWithBringAndroidTabsPrompt:(BOOL)reviewTabs {
  DCHECK(_bringAndroidTabsPromptCoordinator);
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  if (reviewTabs) {
    _tabListFromAndroidCoordinator = [[TabListFromAndroidCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.regularBrowser];
    [_tabListFromAndroidCoordinator start];
  } else {
    // The user journey to bring recent tabs on Android to iOS has finished.
    // Reload the service to update/clear the tabs.
    BringAndroidTabsToIOSServiceFactory::GetForProfileIfExists(
        self.regularBrowser->GetProfile())
        ->LoadTabs();
  }
  [_bringAndroidTabsPromptCoordinator stop];
  _bringAndroidTabsPromptCoordinator = nil;
}

#pragma mark - TabGridCommands

- (void)bringGroupIntoView:(const TabGroup*)group animated:(BOOL)animated {
  TabGridPage pageToOpen;
  if ([_regularGridCoordinator bringTabGroupIntoViewIfPresent:group
                                                     animated:animated]) {
    pageToOpen = TabGridPageRegularTabs;
  } else if ([_incognitoGridCoordinator
                 bringTabGroupIntoViewIfPresent:group
                                       animated:animated]) {
    pageToOpen = TabGridPageIncognitoTabs;
  } else {
    // Tab group is not opened, return;
    return;
  }
  [self.baseViewController setCurrentPageAndPageControl:pageToOpen
                                               animated:YES];
}

- (void)showHistoryForText:(NSString*)text {
  // A history coordinator from main_controller won't work properly from the
  // tab grid. Using a local coordinator works better and we need to set
  // `loadStrategy` to YES to ALWAYS_NEW_FOREGROUND_TAB.
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.regularBrowser];
  self.historyCoordinator.searchTerms = text;
  self.historyCoordinator.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  self.historyCoordinator.presentationDelegate = self;
  self.historyCoordinator.delegate = self;
  [self.historyCoordinator start];
}

- (void)showWebSearchForText:(NSString*)text {
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(
          self.regularBrowser->GetProfile());

  const TemplateURL* searchURLTemplate =
      templateURLService->GetDefaultSearchProvider();
  DCHECK(searchURLTemplate);

  TemplateURLRef::SearchTermsArgs searchArgs(base::SysNSStringToUTF16(text));

  GURL searchURL(searchURLTemplate->url_ref().ReplaceSearchTerms(
      searchArgs, templateURLService->search_terms_data()));
  [self openLinkWithURL:searchURL];
}

- (void)showRecentTabsForText:(NSString*)text {
  [self.baseViewController setCurrentPageAndPageControl:TabGridPageRemoteTabs
                                               animated:YES];
}

- (void)showTabGroupsPanelAnimated:(BOOL)animated {
  CHECK(IsTabGroupSyncEnabled());
  [self.baseViewController setCurrentPageAndPageControl:TabGridPageTabGroups
                                               animated:animated];
}

- (void)exitTabGrid {
  [self.baseViewController updateActivePageToCurrent];
  TabGridPage targetPage = self.baseViewController.activePage;

  // Holding the done button down when it is enabled could result in done tap
  // being triggered on release after tabs have been closed and the button
  // disabled. Ensure that action is only taken on a valid state.
  if (![self tabsPresentForPage:targetPage]) {
    return;
  }
  [self showActiveTabInPage:targetPage focusOmnibox:NO];
}

#pragma mark - SnackbarCoordinatorDelegate

- (CGFloat)snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:
               (SnackbarCoordinator*)snackbarCoordinator
                                                forceBrowserToolbar:
                                                    (BOOL)forceBrowserToolbar {
  if (!self.bvcContainer.currentBVC) {
    // The tab grid is being show so use tab grid bottom bar.
    // kTabGridBottomToolbarGuide is stored in the shared layout guide center.
    UIView* tabGridBottomToolbarView = [LayoutGuideCenterForBrowser(nil)
        referencedViewUnderName:kTabGridBottomToolbarGuide];
    return CGRectGetHeight(tabGridBottomToolbarView.bounds);
  }

  if (!forceBrowserToolbar &&
      self.bvcContainer.currentBVC.presentedViewController) {
    UIViewController* presentedViewController =
        self.bvcContainer.currentBVC.presentedViewController;

    // When the presented view is a navigation controller, return the navigation
    // controller's toolbar height.
    if ([presentedViewController isKindOfClass:UINavigationController.class]) {
      UINavigationController* navigationController =
          base::apple::ObjCCastStrict<UINavigationController>(
              presentedViewController);

      if (navigationController.toolbar &&
          !navigationController.isToolbarHidden) {
        CGFloat toolbarHeight =
            CGRectGetHeight(presentedViewController.view.frame) -
            CGRectGetMinY(navigationController.toolbar.frame);
        return toolbarHeight;
      } else {
        return 0.0;
      }
    }
  }

  // Use the BVC bottom bar as the offset.
  Browser* browser = nil;
  if (snackbarCoordinator == self.snackbarCoordinator) {
    browser = self.regularBrowser;
  } else if (snackbarCoordinator == self.incognitoSnackbarCoordinator) {
    browser = self.incognitoBrowser;
  }
  CHECK(browser);

  UIView* bottomToolbar = [LayoutGuideCenterForBrowser(browser)
      referencedViewUnderName:kSecondaryToolbarGuide];

  return CGRectGetHeight(bottomToolbar.bounds);
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(SigninCoordinatorResult)result {
  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  [self.remoteTabsMediator refreshSessionsView];
}

#pragma mark - TabGroupPositioner

- (UIView*)viewAboveTabGroup {
  return self.bvcContainer.view;
}

#pragma mark - LegacyGridTransitionAnimationLayoutProviding

- (BOOL)isSelectedCellVisible {
  if (self.baseViewController.activePage !=
      self.baseViewController.currentPage) {
    return NO;
  }

  switch (self.baseViewController.activePage) {
    case TabGridPageIncognitoTabs:
      return [_incognitoGridCoordinator isSelectedCellVisible];
    case TabGridPageRegularTabs:
      return [_regularGridCoordinator isSelectedCellVisible];
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      return NO;
  }
}

- (BOOL)shouldReparentSelectedCell:(GridAnimationDirection)animationDirection {
  switch (animationDirection) {
      // For contracting animation only selected pinned cells should be
      // reparented.
    case GridAnimationDirectionContracting:
      return [self isPinnedCellSelected];
      // For expanding animation any selected cell should be reparented.
    case GridAnimationDirectionExpanding:
      return YES;
  }
}

- (LegacyGridTransitionLayout*)transitionLayout:(TabGridPage)activePage {
  LegacyGridTransitionLayout* layout =
      [self transitionLayoutForPage:activePage];
  if (!layout) {
    return nil;
  }
  layout.frameChanged = !CGRectEqualToRect(self.baseViewController.view.frame,
                                           _frameWhenEntering);
  return layout;
}

- (UIView*)animationViewsContainer {
  return self.baseViewController.view;
}

- (UIView*)animationViewsContainerBottomView {
  // The animation should happen just above the direct subview of the TabGrid
  // containing the visible grid.
  UIView* potentialGridContainer;
  switch (self.baseViewController.activePage) {
    case TabGridPageIncognitoTabs:
      potentialGridContainer = [_incognitoGridCoordinator gridView];
      break;
    case TabGridPageRegularTabs:
      potentialGridContainer = [_regularGridCoordinator gridView];
      break;
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      NOTREACHED();
  }
  UIView* baseView = self.baseViewController.view;
  while (potentialGridContainer.superview != baseView) {
    potentialGridContainer = potentialGridContainer.superview;
  }
  return potentialGridContainer;
}

- (CGRect)gridContainerFrame {
  UIView* potentialAnimationContainer;
  switch (self.baseViewController.activePage) {
    case TabGridPageIncognitoTabs:
      potentialAnimationContainer =
          [_incognitoGridCoordinator gridContainerForAnimation];
      break;
    case TabGridPageRegularTabs:
      potentialAnimationContainer =
          [_regularGridCoordinator gridContainerForAnimation];
      break;
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      NOTREACHED();
  }
  if (potentialAnimationContainer) {
    return potentialAnimationContainer.frame;
  }
  return self.baseViewController.view.bounds;
}

// Returns whether there is a selected pinned cell.
- (BOOL)isPinnedCellSelected {
  if (!IsPinnedTabsEnabled() ||
      self.baseViewController.currentPage != TabGridPageRegularTabs) {
    return NO;
  }

  return [_regularGridCoordinator.pinnedTabsViewController hasSelectedCell];
}

// Returns transition layout for the provided `page`.
- (LegacyGridTransitionLayout*)transitionLayoutForPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return [_incognitoGridCoordinator transitionLayout];
    case TabGridPageRegularTabs:
      return [_regularGridCoordinator transitionLayout];
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:
      return nil;
  }
}

@end
