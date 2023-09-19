// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/common/supervised_user_utils.h"
#import "ios/chrome/browser/bookmarks/model/account_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/bring_android_tabs/features.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/find_in_page/util.h"
#import "ios/chrome/browser/main/browser_util.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/reading_list/reading_list_browser_agent.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/distant_session.h"
#import "ios/chrome/browser/synced_sessions/synced_sessions_util.h"
#import "ios/chrome/browser/tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/bookmarks/bookmarks_coordinator.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_prompt_coordinator.h"
#import "ios/chrome/browser/ui/bring_android_tabs/tab_list_from_android_coordinator.h"
#import "ios/chrome/browser/ui/commerce/price_card/price_card_mediator.h"
#import "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/history/history_coordinator_delegate.h"
#import "ios/chrome/browser/ui/history/public/history_presentation_delegate.h"
#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"
#import "ios/chrome/browser/ui/menu/tab_context_menu_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_mediator.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_menu_helper.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_presentation_delegate.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_table_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_button_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/pinned_tabs/pinned_tabs_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_context_menu_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator+private.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_coordinator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_coordinator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_top_toolbar.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/legacy_tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
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
                                  GridMediatorDelegate,
                                  HistoryCoordinatorDelegate,
                                  HistoryPresentationDelegate,
                                  HistorySyncPopupCoordinatorDelegate,
                                  InactiveTabsCoordinatorDelegate,
                                  RecentTabsPresentationDelegate,
                                  SceneStateObserver,
                                  SnackbarCoordinatorDelegate,
                                  TabContextMenuDelegate,
                                  TabGridMediatorDelegate,
                                  TabGridViewControllerDelegate,
                                  TabPresentationDelegate> {
  // Use an explicit ivar instead of synthesizing as the setter isn't using the
  // ivar.
  Browser* _incognitoBrowser;

  // Browser that contain tabs, from the regular browser, that have not been
  // open since a certain amount of time.
  Browser* _inactiveBrowser;

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
}

// Browser that contain tabs from the main pane (i.e. non-incognito).
// TODO(crbug.com/1416934): Make regular ivar as incognito and inactive.
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
@property(nonatomic, strong) RegularGridMediator* regularTabsMediator;
// Mediator for incognito Tabs.
@property(nonatomic, strong) IncognitoGridMediator* incognitoTabsMediator;
// Mediator for PriceCardView - this is only for regular Tabs.
@property(nonatomic, strong) PriceCardMediator* priceCardMediator;
// Mediator for remote Tabs.
@property(nonatomic, strong) RecentTabsMediator* remoteTabsMediator;
// Mediator for pinned Tabs.
@property(nonatomic, strong) PinnedTabsMediator* pinnedTabsMediator;
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

// Helper objects to be provided to the TabGridViewController to create
// the context menu configuration.
@property(nonatomic, strong) TabContextMenuHelper* regularTabContextMenuHelper;
@property(nonatomic, strong)
    TabContextMenuHelper* incognitoTabContextMenuHelper;

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
    browsingDataCommandEndpoint:
        (id<BrowsingDataCommands>)browsingDataCommandEndpoint
                 regularBrowser:(Browser*)regularBrowser
                inactiveBrowser:(Browser*)inactiveBrowser
               incognitoBrowser:(Browser*)incognitoBrowser {
  if ((self = [super initWithBaseViewController:nil browser:nullptr])) {
    _window = window;
    _dispatcher = [[CommandDispatcher alloc] init];
    [_dispatcher startDispatchingToTarget:applicationCommandEndpoint
                              forProtocol:@protocol(ApplicationCommands)];
    // -startDispatchingToTarget:forProtocol: doesn't pick up protocols the
    // passed protocol conforms to, so ApplicationSettingsCommands and
    // BrowsingDataCommands are explicitly dispatched to the endpoint as well.
    [_dispatcher
        startDispatchingToTarget:applicationCommandEndpoint
                     forProtocol:@protocol(ApplicationSettingsCommands)];
    [_dispatcher startDispatchingToTarget:browsingDataCommandEndpoint
                              forProtocol:@protocol(BrowsingDataCommands)];

    _regularBrowser = regularBrowser;
    _inactiveBrowser = inactiveBrowser;
    _incognitoBrowser = incognitoBrowser;

    if (IsIncognitoModeDisabled(
            _regularBrowser->GetBrowserState()->GetPrefs())) {
      _pageConfiguration = TabGridPageConfiguration::kIncognitoPageDisabled;
    } else if (IsIncognitoModeForced(
                   _incognitoBrowser->GetBrowserState()->GetPrefs())) {
      _pageConfiguration = TabGridPageConfiguration::kIncognitoPageOnly;
    } else {
      _pageConfiguration = TabGridPageConfiguration::kAllPagesEnabled;
    }
  }
  return self;
}

#pragma mark - Public

- (Browser*)browser {
  NOTREACHED();
  return nil;
}

- (Browser*)regularBrowser {
  // Ensure browser which is actually used by the mediator is returned, as it
  // may have been updated.
  return self.regularTabsMediator ? self.regularTabsMediator.browser
                                  : _regularBrowser;
}

- (Browser*)incognitoBrowser {
  // Ensure browser which is actually used by the mediator is returned, as it
  // may have been updated.
  return self.incognitoTabsMediator ? self.incognitoTabsMediator.browser
                                    : _incognitoBrowser;
}

- (void)setIncognitoBrowser:(Browser*)incognitoBrowser {
  DCHECK(self.incognitoTabsMediator);
  self.incognitoTabsMediator.browser = incognitoBrowser;
  if (incognitoBrowser) {
    self.incognitoTabContextMenuHelper.browserState =
        incognitoBrowser->GetBrowserState();
  } else {
    self.incognitoTabContextMenuHelper.browserState = nullptr;
  }

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
  }
}

- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion {
  // A modal may be presented on top of the Recent Tabs or tab grid.
  [self.baseViewController dismissModals];
  self.baseViewController.tabGridMode = TabGridModeNormal;

  [self dismissPopovers];

  [self.inactiveTabsCoordinator hide];
  [_toolbarsCoordinator stop];

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

- (void)setActivePage:(TabGridPage)page {
  DCHECK(page != TabGridPageRemoteTabs);
  self.baseViewController.activePage = page;
  [_mediator setPage:page];
}

- (void)setActiveMode:(TabGridMode)mode {
  self.baseViewController.tabGridMode = mode;
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

- (void)prepareToShowTabGrid {
  // No-op if the BVC isn't being presented.
  if (!self.bvcContainer)
    return;
  [base::apple::ObjCCast<TabGridViewController>(self.baseViewController)
      prepareForAppearance];
}

- (void)showTabGrid {
  BOOL animated = !self.animationsDisabledForTesting;

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.regularBrowser)->GetSceneState();
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
        BringAndroidTabsToIOSServiceFactory::GetForBrowserState(
            self.regularBrowser->GetBrowserState());
    if (bringAndroidTabsService != nil) {
      bringAndroidTabsService->LoadTabs();
      shouldDisplayBringAndroidTabsPrompt =
          bringAndroidTabsService->GetNumberOfAndroidTabs() > 0;
    }
  }

  __weak __typeof(self) weakSelf = self;

  ProceduralBlock transitionCompletionBlock = ^{
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    strongSelf.bvcContainer = nil;
    [strongSelf.baseViewController contentDidAppear];

    if (shouldDisplayBringAndroidTabsPrompt) {
      [strongSelf displayBringAndroidTabsPrompt];
    }
  };

  ProceduralBlock transitionBlock = ^{
    __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    if (IsNewTabGridTransitionsEnabled()) {
      [self
          performBrowserToTabGridTransitionWithAnimationEnabled:animated
                                                     completion:
                                                         transitionCompletionBlock];
    } else {
      [strongSelf
          performLegacyBrowserToTabGridTransitionWithActivePage:
              currentActivePage
                                               animationEnabled:animated
                                                     completion:
                                                         transitionCompletionBlock];
    }
    // On iOS 15+, snapshotting views with afterScreenUpdates:YES waits 0.5s
    // for the status bar style to update. Work around that delay by taking
    // the snapshot first (during
    // `transitionFromBrowser:toTabGrid:activePage:withCompletion`) and then
    // updating the status bar style afterwards.
    strongSelf.baseViewController.childViewControllerForStatusBarStyle = nil;
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

  // Record when the tab switcher is presented.
  self.tabGridEnterTime = base::TimeTicks::Now();
  base::RecordAction(base::UserMetricsAction("MobileTabGridEntered"));
  [self.priceCardMediator logMetrics:TAB_SWITCHER];
}

- (void)showTabViewController:(UIViewController*)viewController
                    incognito:(BOOL)incognito
                   completion:(ProceduralBlock)completion {
  DCHECK(viewController || self.bvcContainer);

  if (!self.tabGridEnterTime.is_null()) {
    // Record when the tab switcher is dismissed.
    base::RecordAction(base::UserMetricsAction("MobileTabGridExited"));

    // Record how long the tab switcher was presented.
    base::TimeDelta duration = base::TimeTicks::Now() - self.tabGridEnterTime;
    base::UmaHistogramLongTimes("IOS.TabSwitcher.TimeSpent", duration);
    self.tabGridEnterTime = base::TimeTicks();
  }

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
    if (self.baseViewController.tabGridMode == TabGridModeSearch) {
      // In search mode, the tabgrid mode is not reset before the animation so
      // the animation can start from the correct cell. Once the animation is
      // complete, reset the tab grid mode.
      self.baseViewController.tabGridMode = TabGridModeNormal;
    }
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
  };

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

  // On iOS 15+, snapshotting views with afterScreenUpdates:YES waits 0.5s for
  // the status bar style to update. Work around that delay by taking the
  // snapshot first (during
  // `transitionFromTabGrid:toBrowser:activePage:withCompletion`) and then
  // updating the status bar style afterwards.
  self.baseViewController.childViewControllerForStatusBarStyle =
      self.bvcContainer.currentBVC;
}

#pragma mark - Private

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
  switch (GetBringYourOwnTabsPromptType()) {
    case BringYourOwnTabsPromptType::kHalfSheet:
      [self.baseViewController
          presentViewController:_bringAndroidTabsPromptCoordinator
                                    .viewController
                       animated:YES
                     completion:nil];
      break;
    case BringYourOwnTabsPromptType::kBottomMessage:
      self.baseViewController.regularTabsBottomMessage =
          _bringAndroidTabsPromptCoordinator.viewController;
      break;
    case BringYourOwnTabsPromptType::kDisabled:
      NOTREACHED();
      break;
  }
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

// Performs the legacy Browser to Tab Grid transition.
- (void)
    performLegacyBrowserToTabGridTransitionWithActivePage:
        (TabGridPage)activePage
                                         animationEnabled:(BOOL)animationEnabled
                                               completion:
                                                   (ProceduralBlock)completion {
  self.legacyTransitionHandler =
      [self createTransitionHanlderWithAnimationEnabled:animationEnabled];
  [self.legacyTransitionHandler transitionFromBrowser:self.bvcContainer
                                            toTabGrid:self.baseViewController
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

// Creates a transition handler with `animationEnabled` parameter.
- (LegacyTabGridTransitionHandler*)createTransitionHanlderWithAnimationEnabled:
    (BOOL)animationEnabled {
  LegacyTabGridTransitionHandler* transitionHandler =
      [[LegacyTabGridTransitionHandler alloc]
          initWithLayoutProvider:self.baseViewController];
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

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[TabGridMediator alloc]
      initWithPrefService:self.regularBrowser->GetBrowserState()->GetPrefs()];

  id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);

  TabGridViewController* baseViewController = [[TabGridViewController alloc]
      initWithPageConfiguration:_pageConfiguration];
  baseViewController.handler = applicationCommandsHandler;
  baseViewController.tabPresentationDelegate = self;
  baseViewController.layoutGuideCenter = LayoutGuideCenterForBrowser(nil);
  baseViewController.delegate = self;
  baseViewController.mutator = _mediator;
  _baseViewController = baseViewController;

  _mediator.consumer = _baseViewController;
  _mediator.delegate = self;

  _toolbarsCoordinator =
      [[TabGridToolbarsCoordinator alloc] initWithBaseViewController:nil
                                                             browser:nil];
  _toolbarsCoordinator.searchDelegate = self.baseViewController;
  _toolbarsCoordinator.actionWrangler = self.baseViewController;
  [_toolbarsCoordinator start];
  self.baseViewController.topToolbar = _toolbarsCoordinator.topToolbar;
  self.baseViewController.bottomToolbar = _toolbarsCoordinator.bottomToolbar;

  self.regularTabsMediator = [[RegularGridMediator alloc]
      initWithConsumer:baseViewController.regularTabsConsumer];
  ChromeBrowserState* regularBrowserState =
      _regularBrowser ? _regularBrowser->GetBrowserState() : nullptr;
  WebStateList* regularWebStateList =
      _regularBrowser ? _regularBrowser->GetWebStateList() : nullptr;
  self.priceCardMediator =
      [[PriceCardMediator alloc] initWithWebStateList:regularWebStateList];

  self.regularTabsMediator.browser = _regularBrowser;
  // TODO(crbug.com/1457146): The action wrangler should be the regular grid
  // view controller.
  self.regularTabsMediator.actionWrangler = self.baseViewController;
  self.regularTabsMediator.delegate = self;
  if (regularBrowserState) {
    self.regularTabsMediator.tabRestoreService =
        IOSChromeTabRestoreServiceFactory::GetForBrowserState(
            regularBrowserState);
  }

  if (IsPinnedTabsEnabled()) {
    self.pinnedTabsMediator = [[PinnedTabsMediator alloc]
        initWithConsumer:baseViewController.pinnedTabsConsumer];
    self.pinnedTabsMediator.browser = _regularBrowser;
    baseViewController.pinnedTabsDelegate = self.pinnedTabsMediator;
  }

  // Offer to manage inactive regular tabs iff the regular tabs grid is
  // available. The regular tabs can be disabled by policy, making the grid
  // unavailable.
  if (IsInactiveTabsAvailable() &&
      _pageConfiguration != TabGridPageConfiguration::kIncognitoPageOnly) {
    CHECK(baseViewController.regularTabsConsumer);
    self.inactiveTabsButtonMediator = [[InactiveTabsButtonMediator alloc]
        initWithConsumer:baseViewController.regularTabsConsumer
            webStateList:_inactiveBrowser->GetWebStateList()
             prefService:GetApplicationContext()->GetLocalState()];
  }

  baseViewController.regularTabsDelegate = self.regularTabsMediator;

  baseViewController.regularTabsDragDropHandler = self.regularTabsMediator;
  if (IsPinnedTabsEnabled()) {
    baseViewController.pinnedTabsDragDropHandler = self.pinnedTabsMediator;
  }

  baseViewController.priceCardDataSource = self.priceCardMediator;

  baseViewController.regularTabsShareableItemsProvider =
      self.regularTabsMediator;

  _incognitoGridCoordinator = [[IncognitoGridCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:_incognitoBrowser
                 toolbarsMutator:_toolbarsCoordinator.toolbarsMutator
            gridMediatorDelegate:self];
  // TODO(crbug.com/1457146): Init view controller inside the coordinator. Also
  // it should be a IncognitoViewController instead of a TabGridViewController.
  _incognitoGridCoordinator.incognitoViewController = self.baseViewController;
  [_incognitoGridCoordinator start];
  self.incognitoTabsMediator = _incognitoGridCoordinator.incognitoGridMediator;

  self.baseViewController.incognitoTabsDelegate = self.incognitoTabsMediator;
  self.baseViewController.incognitoTabsDragDropHandler =
      self.incognitoTabsMediator;
  self.baseViewController.incognitoTabsShareableItemsProvider =
      self.incognitoTabsMediator;

  self.recentTabsContextMenuHelper =
      [[RecentTabsContextMenuHelper alloc] initWithBrowser:self.regularBrowser
                            recentTabsPresentationDelegate:self
                                    tabContextMenuDelegate:self];
  self.baseViewController.remoteTabsViewController.menuProvider =
      self.recentTabsContextMenuHelper;

  self.regularTabContextMenuHelper = [[TabContextMenuHelper alloc]
        initWithBrowserState:self.regularBrowser->GetBrowserState()
      tabContextMenuDelegate:self];
  self.baseViewController.regularTabsContextMenuProvider =
      self.regularTabContextMenuHelper;
  self.incognitoTabContextMenuHelper = [[TabContextMenuHelper alloc]
        initWithBrowserState:self.incognitoBrowser->GetBrowserState()
      tabContextMenuDelegate:self];
  self.baseViewController.incognitoTabsContextMenuProvider =
      self.incognitoTabContextMenuHelper;

  if (IsInactiveTabsAvailable()) {
    self.inactiveTabsCoordinator = [[InactiveTabsCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:_inactiveBrowser
                          delegate:self
                      menuProvider:self.regularTabContextMenuHelper];
    [self.inactiveTabsCoordinator start];

    baseViewController.inactiveTabsDelegate =
        self.inactiveTabsCoordinator.gridCommandsHandler;
    self.regularTabsMediator.containedGridToolbarsProvider =
        self.inactiveTabsCoordinator.toolbarsConfigurationProvider;
    self.regularTabsMediator.inactiveTabsGridCommands =
        self.inactiveTabsCoordinator.gridCommandsHandler;
  }

  // TODO(crbug.com/845192) : Remove RecentTabsTableViewController dependency on
  // ChromeBrowserState so that we don't need to expose the view controller.
  baseViewController.remoteTabsViewController.browser = self.regularBrowser;
  sync_sessions::SessionSyncService* syncService =
      SessionSyncServiceFactory::GetForBrowserState(regularBrowserState);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(regularBrowserState);
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(
          regularBrowserState);
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(regularBrowserState);
  syncer::SyncService* service =
      SyncServiceFactory::GetForBrowserState(regularBrowserState);
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(regularBrowserState);
  // TODO(crbug.com/1457146): Rename in recentTabsMediator.
  self.remoteTabsMediator =
      [[RecentTabsMediator alloc] initWithSessionSyncService:syncService
                                             identityManager:identityManager
                                              restoreService:restoreService
                                               faviconLoader:faviconLoader
                                                 syncService:service
                                                 browserList:browserList];
  self.remoteTabsMediator.consumer = baseViewController.remoteTabsConsumer;
  baseViewController.remoteTabsViewController.imageDataSource =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.delegate =
      self.remoteTabsMediator;
  baseViewController.remoteTabsViewController.handler =
      applicationCommandsHandler;
  baseViewController.remoteTabsViewController.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  baseViewController.remoteTabsViewController.presentationDelegate = self;

  self.firstPresentation = YES;

  // TODO(crbug.com/850387) : Currently, consumer calls from the mediator
  // prematurely loads the view in `RecentTabsTableViewController`. Fix this so
  // that the view is loaded only by an explicit placement in the view
  // hierarchy. As a workaround, the view controller hierarchy is loaded here
  // before `RecentTabsMediator` updates are started.
  self.window.rootViewController = self.baseViewController;
  if (regularBrowserState) {
    [self.remoteTabsMediator initObservers];
    [self.remoteTabsMediator refreshSessionsView];
  }

  _mediator.regularPageMutator = self.regularTabsMediator;
  _mediator.incognitoPageMutator = self.incognitoTabsMediator;
  _mediator.remotePageMutator = self.remoteTabsMediator;

  self.regularTabsMediator.toolbarsMutator =
      _toolbarsCoordinator.toolbarsMutator;
  self.remoteTabsMediator.toolbarsMutator =
      _toolbarsCoordinator.toolbarsMutator;

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

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.regularBrowser)->GetSceneState();
  [sceneState addObserver:self];

  // Once the mediators are set up, stop keeping pointers to the browsers used
  // to initialize them.
  _regularBrowser = nil;
  _incognitoBrowser = nil;
}

- (void)stop {
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.regularBrowser)->GetSceneState();
  [sceneState removeObserver:self];

  // The TabGridViewController may still message its application commands
  // handler after this coordinator has stopped; make this action a no-op by
  // setting the handler to nil.
  self.baseViewController.handler = nil;
  self.recentTabsContextMenuHelper = nil;
  self.regularTabContextMenuHelper = nil;
  self.incognitoTabContextMenuHelper = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  [self.dispatcher stopDispatchingForProtocol:@protocol(ApplicationCommands)];
  [self.dispatcher
      stopDispatchingForProtocol:@protocol(ApplicationSettingsCommands)];
  [self.dispatcher stopDispatchingForProtocol:@protocol(BrowsingDataCommands)];

  [_toolbarsCoordinator stop];
  _toolbarsCoordinator = nil;

  [_incognitoGridCoordinator stop];
  _incognitoGridCoordinator = nil;

  [self.regularTabsMediator disconnect];
  self.regularTabsMediator = nil;

  // TODO(crbug.com/845192) : RecentTabsTableViewController behaves like a
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

  self.baseViewController.regularTabsBottomMessage = nil;
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
      NOTREACHED() << "It is invalid to have an active tab in remote tabs.";
      // This appears to come up in release -- see crbug.com/1069243.
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

- (void)
    showCloseItemsConfirmationActionSheetWithBaseGridMediator:
        (BaseGridMediator*)baseGridMediator
                                                        items:(NSArray<
                                                                  NSString*>*)
                                                                  items
                                                       anchor:(UIBarButtonItem*)
                                                                  buttonAnchor {
  if (baseGridMediator == self.regularTabsMediator) {
    base::RecordAction(base::UserMetricsAction(
        "MobileTabGridSelectionCloseRegularTabsConfirmationPresented"));

    self.actionSheetCoordinator = [[ActionSheetCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.regularBrowser
                             title:nil
                           message:nil
                     barButtonItem:buttonAnchor];
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
  [self.actionSheetCoordinator
      addItemWithTitle:base::SysUTF16ToNSString(
                           l10n_util::GetPluralStringFUTF16(
                               IDS_IOS_TAB_GRID_CLOSE_ALL_TABS_CONFIRMATION,
                               items.count))
                action:^{
                  base::RecordAction(base::UserMetricsAction(
                      "MobileTabGridSelectionCloseTabsConfirmed"));
                  [weakBaseGridMediator closeItemsWithIDs:items];
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

#pragma mark - TabGridMediatorDelegate

// TODO(crbug.com/1457146): Move this to incognito grid coordinator once it is
// created.
- (void)updateIncognitoTabGridState {
  // Reconnect the incognito mediators to the incognito view controller.
  self.incognitoTabsMediator.consumer =
      self.baseViewController.incognitoTabsConsumer;

  // Reset the connection between the incognito view controller and the
  // mediator.
  self.baseViewController.incognitoTabsContextMenuProvider =
      self.incognitoTabContextMenuHelper;
  self.baseViewController.incognitoTabsShareableItemsProvider =
      self.incognitoTabsMediator;
}

#pragma mark - TabGridViewControllerDelegate

- (TabGridPage)activePageForTabGridViewController:
    (TabGridViewController*)tabGridViewController {
  return [self.delegate activePageForTabGrid:self];
}

- (void)tabGridViewControllerDidDismiss:
    (TabGridViewController*)tabGridViewController {
  [self.delegate tabGridDismissTransitionDidEnd:self];
}

- (void)openLinkWithURL:(const GURL&)URL {
  id<ApplicationCommands> handler =
      HandlerForProtocol(self.dispatcher, ApplicationCommands);
  [handler openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:URL]];
}

- (void)setBVCAccessibilityViewModal:(BOOL)modal {
  self.bvcContainer.view.accessibilityViewIsModal = modal;
}

- (void)openSearchResultsPageForSearchText:(NSString*)searchText {
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.regularBrowser->GetBrowserState());

  const TemplateURL* searchURLTemplate =
      templateURLService->GetDefaultSearchProvider();
  DCHECK(searchURLTemplate);

  TemplateURLRef::SearchTermsArgs searchArgs(
      base::SysNSStringToUTF16(searchText));

  GURL searchURL(searchURLTemplate->url_ref().ReplaceSearchTerms(
      searchArgs, templateURLService->search_terms_data()));
  [self openLinkWithURL:searchURL];
}

- (void)showHistoryFilteredBySearchText:(NSString*)searchText {
  // A history coordinator from main_controller won't work properly from the
  // tab grid. Using a local coordinator works better and we need to set
  // `loadStrategy` to YES to ALWAYS_NEW_FOREGROUND_TAB.
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.regularBrowser];
  self.historyCoordinator.searchTerms = searchText;
  self.historyCoordinator.loadStrategy =
      UrlLoadStrategy::ALWAYS_NEW_FOREGROUND_TAB;
  self.historyCoordinator.presentationDelegate = self;
  self.historyCoordinator.delegate = self;
  [self.historyCoordinator start];
}

- (void)showInactiveTabs {
  CHECK(IsInactiveTabsEnabled());
  [self.inactiveTabsCoordinator show];
}

#pragma mark - InactiveTabsCoordinatorDelegate

- (void)inactiveTabsCoordinator:
            (InactiveTabsCoordinator*)inactiveTabsCoordinator
            didSelectItemWithID:(NSString*)itemID {
  WebStateList* regularWebStateList = self.regularBrowser->GetWebStateList();
  int toInsertIndex = regularWebStateList->count();

  MoveTabToBrowser(itemID, self.regularBrowser, toInsertIndex);

  // TODO(crbug.com/1420938): Adapt the animation so the grid animation is
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
  [self showHistoryFilteredBySearchText:searchTerms];
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

  BOOL inIncognito = self.regularBrowser->GetBrowserState()->IsOffTheRecord();
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
  bookmarks::BookmarkModel* localOrSyncableBookmarkModel =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          self.regularBrowser->GetBrowserState());
  bookmarks::BookmarkModel* accountBookmarkModel =
      ios::AccountBookmarkModelFactory::GetForBrowserState(
          self.regularBrowser->GetBrowserState());
  if (bookmark_utils_ios::IsBookmarked(URL, localOrSyncableBookmarkModel,
                                       accountBookmarkModel)) {
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

- (void)pinTabWithIdentifier:(NSString*)identifier {
  [self.regularTabsMediator setPinState:YES forItemWithIdentifier:identifier];
}

- (void)unpinTabWithIdentifier:(NSString*)identifier {
  [self.pinnedTabsMediator setPinState:NO forItemWithIdentifier:identifier];
}

- (void)closeTabWithIdentifier:(NSString*)identifier
                     incognito:(BOOL)incognito
                        pinned:(BOOL)pinned {
  if (incognito) {
    [self.incognitoTabsMediator closeItemWithID:identifier];
    return;
  }

  if (pinned) {
    DCHECK(IsPinnedTabsEnabled());
    [self.pinnedTabsMediator closeItemWithID:identifier];
    return;
  }

  [self.regularTabsMediator closeItemWithID:identifier];
}

- (void)selectTabs {
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridTabContextMenuSelectTabs"));
  self.baseViewController.tabGridMode = TabGridModeSelection;
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
  switch (GetBringYourOwnTabsPromptType()) {
    case BringYourOwnTabsPromptType::kHalfSheet:
      [self.baseViewController dismissViewControllerAnimated:YES
                                                  completion:nil];
      break;
    case BringYourOwnTabsPromptType::kBottomMessage:
      DCHECK_EQ(self.baseViewController.regularTabsBottomMessage,
                _bringAndroidTabsPromptCoordinator.viewController);
      self.baseViewController.regularTabsBottomMessage = nil;
      break;
    case BringYourOwnTabsPromptType::kDisabled:
      NOTREACHED();
      break;
  }
  if (reviewTabs) {
    _tabListFromAndroidCoordinator = [[TabListFromAndroidCoordinator alloc]
        initWithBaseViewController:self.baseViewController
                           browser:self.regularBrowser];
    [_tabListFromAndroidCoordinator start];
  } else {
    // The user journey to bring recent tabs on Android to iOS has finished.
    // Reload the service to update/clear the tabs.
    BringAndroidTabsToIOSServiceFactory::GetForBrowserStateIfExists(
        self.regularBrowser->GetBrowserState())
        ->LoadTabs();
  }
  [_bringAndroidTabsPromptCoordinator stop];
  _bringAndroidTabsPromptCoordinator = nil;
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

@end
