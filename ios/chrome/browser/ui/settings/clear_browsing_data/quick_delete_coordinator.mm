// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/tabs_animation_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_browsing_data_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mediator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using browsing_data::DeleteBrowsingDataDialogAction;

}  // namespace

@interface QuickDeleteCoordinator () <QuickDeleteBrowsingDataDelegate,
                                      QuickDeletePresentationCommands,
                                      UIAdaptivePresentationControllerDelegate>
@end

@implementation QuickDeleteCoordinator {
  QuickDeleteViewController* _viewController;
  QuickDeleteMediator* _mediator;
  QuickDeleteBrowsingDataCoordinator* _browsingDataCoordinator;
  std::unique_ptr<ScopedUIBlocker> _windowUIBlocker;

  // The tabs closure animation should only be performed if Quick Delete is
  // opened on top of a tab or the tab grid.
  BOOL _canPerformTabsClosureAnimation;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
            canPerformTabsClosureAnimation:
                (BOOL)canPerformTabsClosureAnimation {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _canPerformTabsClosureAnimation = canPerformTabsClosureAnimation;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  CHECK(!browserState->IsOffTheRecord());

  BrowsingDataCounterWrapperProducer* producer =
      [[BrowsingDataCounterWrapperProducer alloc]
          initWithBrowserState:browserState];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(browserState);
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserState(browserState);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForBrowserState(browserState);

  _mediator = [[QuickDeleteMediator alloc]
                           initWithPrefs:browserState->GetPrefs()
      browsingDataCounterWrapperProducer:producer
                         identityManager:identityManager
                     browsingDataRemover:browsingDataRemover
                     discoverFeedService:discoverFeedService
          canPerformTabsClosureAnimation:_canPerformTabsClosureAnimation];

  _viewController = [[QuickDeleteViewController alloc] init];
  _viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  _mediator.consumer = _viewController;
  _mediator.presentationHandler = self;

  _viewController.presentationHandler = self;
  _viewController.mutator = _mediator;
  _viewController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self animateDismissalWithCompletion:nil];
  [self disconnect];
}

#pragma mark - QuickDeletePresentationCommands

- (void)dismissQuickDelete {
  id<QuickDeleteCommands> quickDeleteHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), QuickDeleteCommands);
  [quickDeleteHandler stopQuickDelete];
}

- (void)openMyActivityURL:(const GURL&)URL {
  if (URL == GURL(kClearBrowsingDataDSESearchUrlInFooterURL)) {
    base::UmaHistogramEnumeration("Settings.ClearBrowsingData.OpenMyActivity",
                                  MyActivityNavigation::kSearchHistory);
    base::UmaHistogramEnumeration(
        browsing_data::kDeleteBrowsingDataDialogHistogram,
        DeleteBrowsingDataDialogAction::kSearchHistoryLinkOpened);
  } else if (URL == GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL)) {
    base::UmaHistogramEnumeration("Settings.ClearBrowsingData.OpenMyActivity",
                                  MyActivityNavigation::kTopLevel);
    base::UmaHistogramEnumeration(
        browsing_data::kDeleteBrowsingDataDialogHistogram,
        DeleteBrowsingDataDialogAction::kMyActivityLinkedOpened);
  } else {
    NOTREACHED();
  }

  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [handler closeSettingsUIAndOpenURL:command];
}

- (void)showBrowsingDataPage {
  [_browsingDataCoordinator stop];

  QuickDeleteBrowsingDataCoordinator* browsingDataCoordinator =
      [[QuickDeleteBrowsingDataCoordinator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser];
  _browsingDataCoordinator = browsingDataCoordinator;
  [_browsingDataCoordinator start];
  _browsingDataCoordinator.delegate = self;
}

- (void)triggerTabsClosureAnimationWithBeginTime:(base::Time)beginTime
                                         endTime:(base::Time)endTime
                                  cachedTabsInfo:
                                      (tabs_closure_util::WebStateIDToTime)
                                          cachedTabsInfo
                            forceWebStatesReload:(BOOL)forceWebStatesReload {
  CHECK(_canPerformTabsClosureAnimation);
  CHECK_EQ(Browser::Type::kRegular, self.browser->type());

  // Get the active and inactive WebStates and the TabGroups of WebStates with a
  // last navigation timestamp between `beginTime` and `endTime`. This
  // information will be used by the tabs closure animation.
  // TODO(crbug.com/335387869): Consider only returning tabs not in tab groups
  // for `activeTabsToClose`.
  std::set<web::WebStateID> activeTabsToClose =
      tabs_closure_util::GetTabsToClose(self.browser->GetWebStateList(),
                                        beginTime, endTime, cachedTabsInfo);
  std::map<tab_groups::TabGroupId, std::set<int>> tabGroupsWithTabsToClose =
      tabs_closure_util::GetTabGroupsWithTabsToClose(
          self.browser->GetWebStateList(), beginTime, endTime, cachedTabsInfo);

  BOOL allInactiveTabsWillClose = NO;
  if (Browser* inactiveBrowser = self.browser->GetInactiveBrowser()) {
    std::set<web::WebStateID> inactiveTabsToClose =
        tabs_closure_util::GetTabsToClose(inactiveBrowser->GetWebStateList(),
                                          beginTime, endTime, cachedTabsInfo);

    allInactiveTabsWillClose = inactiveBrowser->GetWebStateList()->count() ==
                               (int)inactiveTabsToClose.size();
  }

  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  browsingDataRemover->SetCachedTabsInfo(cachedTabsInfo);

  BrowsingDataRemover::RemovalParams params{
      .show_activity_indicator =
          BrowsingDataRemover::ActivityIndicatorPolicy::kNoIndicator,
  };

  if (forceWebStatesReload) {
    params.reload_web_states =
        BrowsingDataRemover::WebStatesReloadPolicy::kForceReload;
  }

  __weak QuickDeleteCoordinator* weakSelf = self;
  ProceduralBlock dismissCompletionBlock = ^() {
    [weakSelf animateTabsClosureWithBeginTime:beginTime
                                      endTime:endTime
                                   activeTabs:activeTabsToClose
                                       groups:tabGroupsWithTabsToClose
                              allInactiveTabs:allInactiveTabsWillClose
                          browsingDataRemover:browsingDataRemover
                    browsingDataRemoverParams:params];
  };

  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommandsHandler
      displayTabGridInMode:TabGridOpeningMode::kRegular];

  // Dismissal of the bottom sheet needs to be dispatched asyncronously so the
  // animation to the tab grid and the dismissal animation are not in conflict,
  // and as such avoiding jittering.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](__typeof(self) strongSelf,
             ProceduralBlock dismissCompletionBlock) {
            [strongSelf animateDismissalWithCompletion:dismissCompletionBlock];
            [strongSelf disconnect];
          },
          weakSelf, dismissCompletionBlock));
}

- (void)blockOtherWindows {
  SceneState* sceneState = self.browser->GetSceneState();
  _windowUIBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
}

- (void)releaseOtherWindows {
  _windowUIBlocker.reset();
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      DeleteBrowsingDataDialogAction::kDialogDismissedImplicitly);

  [self disconnect];
  [self dismissQuickDelete];
}

#pragma mark - QuickDeleteBrowsingDataDelegate

- (void)stopBrowsingDataPage {
  [_browsingDataCoordinator stop];
  _browsingDataCoordinator = nil;

  // Move Voiceover focus to the browsing data row.
  [_viewController focusOnBrowsingDataRow];
}

#pragma mark - Private

// Dismisses the `_viewController` with animation. `completionBlock` is run when
// the dismissal has completed.
- (void)animateDismissalWithCompletion:(ProceduralBlock)completionBlock {
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completionBlock];
}

// Triggers the tabs closure animation on the tab grid for the WebStates in
// `tabsToClose`, for the groups in `groupsWithTabsToClose`, and if
// `animateAllInactiveTabs` is true, then for the inactive tabs banner. It also
// closes all WebStates with a last navigation between [`beginTime`, `endTime`[
// in all browsers through `browsingDataRemover` after the animation has run.
- (void)
    animateTabsClosureWithBeginTime:(base::Time)beginTime
                            endTime:(base::Time)endTime
                         activeTabs:(std::set<web::WebStateID>)activeTabsToClose
                             groups:(std::map<tab_groups::TabGroupId,
                                              std::set<int>>)
                                        tabGroupsWithTabsToClose
                    allInactiveTabs:(BOOL)animateAllInactiveTabs
                browsingDataRemover:(BrowsingDataRemover*)browsingDataRemover
          browsingDataRemoverParams:(BrowsingDataRemover::RemovalParams)params {
  base::OnceClosure onRemoverCompletion = base::BindOnce(
      [](UIWindow* window, std::unique_ptr<ScopedUIBlocker> uiBlocker) {
        uiBlocker.reset();
        window.userInteractionEnabled = YES;
        window.accessibilityElementsHidden = NO;

        // Inform Voiceover users that their browsing data has been deleted.
        // Otherwise, users will only hear that the deletion is in progress.
        // Also make Voiceover focus on the first element of the new page.
        UIAccessibilityPostNotification(
            UIAccessibilityScreenChangedNotification,
            l10n_util::GetNSString(
                IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE));
        // Add vibration at the end of the animation including after the tabs
        // rearrange.
        TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
      },
      self.baseViewController.view.window, std::move(_windowUIBlocker));

  base::OnceClosure onAnimationCompletion = base::BindOnce(
      &BrowsingDataRemover::RemoveInRange, browsingDataRemover->AsWeakPtr(),
      beginTime, endTime, BrowsingDataRemoveMask::CLOSE_TABS,
      std::move(onRemoverCompletion), params);

  id<TabsAnimationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), TabsAnimationCommands);

  [handler animateTabsClosureForTabs:activeTabsToClose
                              groups:tabGroupsWithTabsToClose
                     allInactiveTabs:animateAllInactiveTabs
                   completionHandler:base::CallbackToBlock(
                                         std::move(onAnimationCompletion))];

  // Shutdown this coordinator. We can do this right away since all the
  // callbacks don't depend on this coordinator being alive.
  [self dismissQuickDelete];
}

// Disconnects all instances.
- (void)disconnect {
  if (_windowUIBlocker) {
    _windowUIBlocker.reset();
  }
  _viewController.presentationHandler = nil;
  _viewController.mutator = nil;
  _viewController.presentationController.delegate = nil;
  _viewController = nil;

  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;

  _browsingDataCoordinator.delegate = nil;
  [_browsingDataCoordinator stop];
  _browsingDataCoordinator = nil;
}

@end
