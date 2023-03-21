// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/ntp_tiles/most_visited_sites.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/favicon/large_icon_cache.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Kill-switch for quick fix of crbug.com/1204507
BASE_FEATURE(kNoRecentTabIfNullWebState,
             "NoRecentTabIfNullWebState",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

@interface ContentSuggestionsCoordinator () <
    ContentSuggestionsMenuProvider,
    ContentSuggestionsViewControllerAudience> {
  // Observer bridge for mediator to listen to
  // StartSurfaceRecentTabObserverBridge.
  std::unique_ptr<StartSurfaceRecentTabObserverBridge> _startSurfaceObserver;
}
@property(nonatomic, strong)
    ContentSuggestionsViewController* contentSuggestionsViewController;
@property(nonatomic, strong) ActionSheetCoordinator* alertCoordinator;
@property(nonatomic, assign) BOOL contentSuggestionsEnabled;
// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;
// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;
// Redefined to not be readonly.
@property(nonatomic, strong)
    ContentSuggestionsMediator* contentSuggestionsMediator;

@end

@implementation ContentSuggestionsCoordinator

- (void)start {
  DCHECK(self.browser);
  if (self.started) {
    // Prevent this coordinator from being started twice in a row
    return;
  }

  _started = YES;

  // Make sure that the omnibox is unfocused to prevent having it visually
  // focused while the NTP is just created (with the fakebox visible).
  id<OmniboxCommands> omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandHandler cancelOmniboxEdit];

  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());

  PrefService* prefs =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();

  self.contentSuggestionsEnabled =
      prefs->GetBoolean(prefs::kArticlesForYouEnabled) &&
      prefs->GetBoolean(prefs::kNTPContentSuggestionsEnabled);

  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  std::unique_ptr<ntp_tiles::MostVisitedSites> mostVisitedFactory =
      IOSMostVisitedSitesFactory::NewForBrowserState(
          self.browser->GetBrowserState());
  ReadingListModel* readingListModel =
      ReadingListModelFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  BOOL isGoogleDefaultSearchProvider =
      [self.ntpDelegate isGoogleDefaultSearchEngine];

  self.contentSuggestionsMediator = [[ContentSuggestionsMediator alloc]
           initWithLargeIconService:largeIconService
                     largeIconCache:cache
                    mostVisitedSite:std::move(mostVisitedFactory)
                   readingListModel:readingListModel
                        prefService:prefs
      isGoogleDefaultSearchProvider:isGoogleDefaultSearchProvider
                            browser:self.browser];
  self.contentSuggestionsMediator.feedDelegate = self.feedDelegate;
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.contentSuggestionsMediator.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCoordinatorCommands,
                     OmniboxCommands, SnackbarCommands>>(
          self.browser->GetCommandDispatcher());
  self.contentSuggestionsMediator.webStateList =
      self.browser->GetWebStateList();
  self.contentSuggestionsMediator.webState = self.webState;

  self.contentSuggestionsViewController =
      [[ContentSuggestionsViewController alloc] init];
  self.contentSuggestionsViewController.suggestionCommandHandler =
      self.contentSuggestionsMediator;
  self.contentSuggestionsViewController.audience = self;
  self.contentSuggestionsViewController.menuProvider = self;
  self.contentSuggestionsViewController.urlLoadingBrowserAgent =
      UrlLoadingBrowserAgent::FromBrowser(self.browser);

  self.contentSuggestionsMediator.consumer =
      self.contentSuggestionsViewController;
}

- (void)stop {
    // Reset the observer bridge object before setting
    // `contentSuggestionsMediator` nil.
    if (_startSurfaceObserver) {
    StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
        ->RemoveObserver(_startSurfaceObserver.get());
    _startSurfaceObserver.reset();
    }
  [self.contentSuggestionsMediator disconnect];
  self.contentSuggestionsMediator = nil;
  self.contentSuggestionsViewController = nil;
  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;
  _started = NO;
}

- (UIViewController*)viewController {
  return self.contentSuggestionsViewController;
}

#pragma mark - Setters

- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
  self.contentSuggestionsMediator.webState = webState;
}

#pragma mark - ContentSuggestionsViewControllerAudience

- (void)viewWillDisappear {
  DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState())
      ->SetIsShownOnStartSurface(false);
  if (ShouldShowReturnToMostRecentTabForStartSurface()) {
    [self.contentSuggestionsMediator hideRecentTabTile];
  }
}

- (void)returnToRecentTabWasAdded {
  [self.ntpDelegate updateFeedLayout];
  [self.ntpDelegate setContentOffsetToTop];
}

- (void)moduleWasRemoved {
  [self.ntpDelegate updateFeedLayout];
}

- (UIEdgeInsets)safeAreaInsetsForDiscoverFeed {
  return [SceneStateBrowserAgent::FromBrowser(self.browser)
              ->GetSceneState()
              .window.rootViewController.view safeAreaInsets];
}

#pragma mark - Public methods

- (UIView*)view {
  return self.contentSuggestionsViewController.view;
}

- (void)reload {
  [self.contentSuggestionsMediator reloadAllData];
}

#pragma mark - ContentSuggestionsMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (ContentSuggestionsMostVisitedItem*)item
                                                      fromView:(UIView*)view {
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        ContentSuggestionsCoordinator* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenarioHistogram::kMostVisitedEntry);

        BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
            initWithBrowser:strongSelf.browser
                   scenario:MenuScenarioHistogram::kMostVisitedEntry];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        CGPoint centerPoint = [view.superview convertPoint:view.center
                                                    toView:nil];

        [menuElements addObject:[actionFactory actionToOpenInNewTabWithBlock:^{
                        [weakSelf.contentSuggestionsMediator
                            openNewTabWithMostVisitedItem:item
                                                incognito:NO
                                                  atIndex:item.index
                                                fromPoint:centerPoint];
                      }]];

        UIAction* incognitoAction =
            [actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
              [weakSelf.contentSuggestionsMediator
                  openNewTabWithMostVisitedItem:item
                                      incognito:YES
                                        atIndex:item.index
                                      fromPoint:centerPoint];
            }];

        if (IsIncognitoModeDisabled(
                self.browser->GetBrowserState()->GetPrefs())) {
          // Disable the "Open in Incognito" option if the incognito mode is
          // disabled.
          incognitoAction.attributes = UIMenuElementAttributesDisabled;
        }

        [menuElements addObject:incognitoAction];

        if (base::ios::IsMultipleScenesSupported()) {
          UIAction* newWindowAction = [actionFactory
              actionToOpenInNewWindowWithURL:item.URL
                              activityOrigin:
                                  WindowActivityContentSuggestionsOrigin];
          [menuElements addObject:newWindowAction];
        }

        [menuElements addObject:[actionFactory actionToCopyURL:item.URL]];

        [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                        [weakSelf shareURL:item.URL
                                     title:item.title
                                  fromView:view];
                      }]];

        [menuElements addObject:[actionFactory actionToRemoveWithBlock:^{
                        [weakSelf.contentSuggestionsMediator
                            removeMostVisited:item];
                      }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };
  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - Helpers

- (void)configureStartSurfaceIfNeeded {
  SceneState* scene =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  if (!NewTabPageTabHelper::FromWebState(self.webState)
           ->ShouldShowStartSurface()) {
    return;
  }

  if (ShouldShowReturnToMostRecentTabForStartSurface()) {
    web::WebState* most_recent_tab =
        StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
            ->most_recent_tab();
    // TODO(crbug.com/1204507): Fix reproduced steps that produce state where
    // most_recent_tab is null but ShouldShowStartSurface() is YES.
    if (!base::FeatureList::IsEnabled(kNoRecentTabIfNullWebState) ||
        most_recent_tab) {
      base::RecordAction(base::UserMetricsAction(
          "IOS.StartSurface.ShowReturnToRecentTabTile"));
      DiscoverFeedServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->SetIsShownOnStartSurface(true);
      NSString* time_label = GetRecentTabTileTimeLabelForSceneState(scene);
      [self.contentSuggestionsMediator
          configureMostRecentTabItemWithWebState:most_recent_tab
                                       timeLabel:time_label];
      if (!_startSurfaceObserver) {
        _startSurfaceObserver =
            std::make_unique<StartSurfaceRecentTabObserverBridge>(
                self.contentSuggestionsMediator);
        StartSurfaceRecentTabBrowserAgent::FromBrowser(self.browser)
            ->AddObserver(_startSurfaceObserver.get());
      }
    }
  }
  if (ShouldShrinkLogoForStartSurface()) {
    base::RecordAction(base::UserMetricsAction("IOS.StartSurface.ShrinkLogo"));
  }
  if (ShouldHideShortcutsForStartSurface()) {
    base::RecordAction(
        base::UserMetricsAction("IOS.StartSurface.HideShortcuts"));
  }
}

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// origin `view` representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::MostVisitedEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.contentSuggestionsViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

@end
