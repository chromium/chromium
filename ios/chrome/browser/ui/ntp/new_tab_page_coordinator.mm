// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/feed/core/v2/public/ios/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/default_search_manager.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_observer_bridge.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/discover_feed/feed_model_configuration.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_synchronizer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_coordinator.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/main/scene_state_observer.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_preview_delegate.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_navigation_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_menu_commands.h"
#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section_coordinator.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/ntp/notification_promo_whats_new.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/follow/follow_provider.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

}  // namespace

@interface NewTabPageCoordinator () <AppStateObserver,
                                     BooleanObserver,
                                     ContentSuggestionsHeaderCommands,
                                     DiscoverFeedDelegate,
                                     DiscoverFeedObserverBridgeDelegate,
                                     DiscoverFeedPreviewDelegate,
                                     DiscoverFeedWrapperViewControllerDelegate,
                                     FeedControlDelegate,
                                     FeedManagementNavigationDelegate,
                                     FeedMenuCommands,
                                     NewTabPageContentDelegate,
                                     NewTabPageDelegate,
                                     NewTabPageFollowDelegate,
                                     OverscrollActionsControllerDelegate,
                                     PrefObserverDelegate,
                                     SceneStateObserver> {
  // Helper object managing the availability of the voice search feature.
  VoiceSearchAvailability _voiceSearchAvailability;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Observes changes in the DiscoverFeed.
  std::unique_ptr<DiscoverFeedObserverBridge> _discoverFeedObserverBridge;
}

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* ntpViewController;

// Mediator owned by this Coordinator.
@property(nonatomic, strong) NTPHomeMediator* ntpMediator;

// View controller wrapping the Discover feed.
@property(nonatomic, strong)
    DiscoverFeedWrapperViewController* discoverFeedWrapperViewController;

// View controller for the incognito NTP.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

// The timetick of the last time the NTP was displayed.
@property(nonatomic, assign) base::TimeTicks didAppearTime;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
@property(nonatomic, assign) BOOL visible;

// Whether the view is new tab view is currently presented (possibly in
// background). Used to report NTP usage metrics.
@property(nonatomic, assign) BOOL viewPresented;

// Wheter the scene is currently in foreground.
@property(nonatomic, assign) BOOL sceneInForeground;

// Handles interactions with the content suggestions header and the fake
// omnibox.
@property(nonatomic, strong)
    ContentSuggestionsHeaderSynchronizer* headerSynchronizer;

// The ViewController displayed by this Coordinator. This is the returned
// ViewController and will contain the |containedViewController| (Which can
// change depending on Feed visibility).
@property(nonatomic, strong) UIViewController* containerViewController;

// The coordinator contained ViewController.
@property(nonatomic, strong) UIViewController* containedViewController;

// PrefService used by this Coordinator.
@property(nonatomic, assign) PrefService* prefService;

// Whether the feed is expanded or collapsed. Collapsed
// means the feed header is shown, but not any of the feed content.
@property(nonatomic, strong) PrefBackedBoolean* feedExpandedPref;

// The view controller representing the Discover feed.
@property(nonatomic, weak) UIViewController* discoverFeedViewController;

// The Coordinator to display previews for Discover feed websites. It also
// handles the actions related to them.
@property(nonatomic, strong) LinkPreviewCoordinator* linkPreviewCoordinator;

// The view controller representing the NTP feed header.
@property(nonatomic, strong) FeedHeaderViewController* feedHeaderViewController;

// Alert coordinator for handling the feed header menu.
@property(nonatomic, strong) ActionSheetCoordinator* alertCoordinator;

// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;

// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// DiscoverFeed Service to display the Feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;

// Metrics recorder for actions relating to the feed.
@property(nonatomic, strong) FeedMetricsRecorder* feedMetricsRecorder;

// The header view controller containing the fake omnibox and logo.
@property(nonatomic, strong)
    ContentSuggestionsHeaderViewController* headerController;

// The coordinator for handling feed management.
@property(nonatomic, strong)
    FeedManagementCoordinator* feedManagementCoordinator;

// Coordinator for Feed top section.
@property(nonatomic, strong)
    FeedTopSectionCoordinator* feedTopSectionCoordinator;

@end

@implementation NewTabPageCoordinator

// Synthesize NewTabPageConfiguring properties.
@synthesize selectedFeed = _selectedFeed;
@synthesize shouldScrollIntoFeed = _shouldScrollIntoFeed;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _containerViewController = [[UIViewController alloc] init];
  }
  return self;
}

- (void)start {
  if (self.started)
    return;

  DCHECK(self.browser);
  DCHECK(self.webState);
  DCHECK(self.toolbarDelegate);
  DCHECK(!self.contentSuggestionsCoordinator);

  // Configures incognito NTP if user is in incognito mode.
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    self.started = YES;
    return;
  }

  // Gets services.
  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.templateURLService = ios::TemplateURLServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.discoverFeedService = DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.prefService =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();

  self.ntpViewController = [[NewTabPageViewController alloc] init];
  self.ntpMediator = [self createNTPMediator];

  // Start observing Prefs.
  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(_prefService);
  _prefObserverBridge.reset(new PrefObserverBridge(self));
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kArticlesForYouEnabled, _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kNTPContentSuggestionsEnabled, _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      _prefChangeRegistrar.get());
  self.feedExpandedPref = [[PrefBackedBoolean alloc]
      initWithPrefService:_prefService
                 prefName:feed::prefs::kArticlesListVisible];
  // Observer is necessary for multiwindow NTPs to remain in sync.
  [self.feedExpandedPref setObserver:self];

  // Start observing DiscoverFeedService.
  _discoverFeedObserverBridge = std::make_unique<DiscoverFeedObserverBridge>(
      self, self.discoverFeedService);

  self.feedMetricsRecorder = self.discoverFeedService->GetFeedMetricsRecorder();
  self.feedMetricsRecorder.feedControlDelegate = self;
  self.feedMetricsRecorder.followDelegate = self;

  if (IsContentSuggestionsHeaderMigrationEnabled()) {
    self.headerController =
        [[ContentSuggestionsHeaderViewController alloc] init];
    // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
    // clean up.
    self.headerController.dispatcher =
        static_cast<id<ApplicationCommands, BrowserCommands, OmniboxCommands,
                       FakeboxFocuser>>(self.browser->GetCommandDispatcher());
    self.headerController.commandHandler = self;
    self.headerController.delegate = self.ntpMediator;

    self.headerController.readingListModel =
        ReadingListModelFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    self.headerController.toolbarDelegate = self.toolbarDelegate;
    self.ntpMediator.consumer = self.headerController;
    self.headerController.baseViewController = self.baseViewController;

    // Only handle app state for the new First Run UI.
    if (base::FeatureList::IsEnabled(kEnableFREUIModuleIOS)) {
      SceneState* sceneState =
          SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
      AppState* appState = sceneState.appState;
      [appState addObserver:self];

      // Do not focus on omnibox for voice over if there are other screens to
      // show.
      if (appState.initStage < InitStageFinal) {
        self.headerController.focusOmniboxWhenViewAppears = NO;
      }
    }
  }

  if (IsDiscoverFeedTopSyncPromoEnabled()) {
    self.feedTopSectionCoordinator = [self createFeedTopSectionCoordinator];
  }

  self.contentSuggestionsCoordinator =
      [self createContentSuggestionsCoordinator];

  if (IsContentSuggestionsHeaderMigrationEnabled()) {
    self.headerController.promoCanShow =
        [self.contentSuggestionsCoordinator notificationPromo]->CanShow();
  }

  // Fetches feed header and conditionally fetches feed. Feed can only be
  // visible if feed header is visible.
  if ([self isFeedHeaderVisible]) {
    [self configureFeedAndHeader];
  }

  [self configureNTPViewController];

  base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState addObserver:self];
  self.sceneInForeground =
      sceneState.activationLevel >= SceneActivationLevelForegroundInactive;

  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  self.viewPresented = NO;
  [self updateVisible];

  if (!IsContentSuggestionsHeaderMigrationEnabled()) {
    // Unfocus omnibox, to prevent it from lingering when it should be dismissed
    // (for example, when navigating away or when changing feed visibility).
    id<OmniboxCommands> omniboxCommandHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), OmniboxCommands);
    [omniboxCommandHandler cancelOmniboxEdit];
  }

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState removeObserver:self];

  [self.feedManagementCoordinator stop];
  self.feedManagementCoordinator = nil;
  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.headerSynchronizer = nil;
  self.headerController = nil;
  self.incognitoViewController = nil;
  // Remove before nil to ensure View Hierarchy doesn't hold last strong
  // reference.
  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];
  self.containedViewController = nil;
  self.ntpViewController = nil;
  self.feedHeaderViewController = nil;
  self.alertCoordinator = nil;
  self.authService = nil;
  self.templateURLService = nil;

  [self.ntpMediator shutdown];
  self.ntpMediator = nil;

  if (self.discoverFeedViewController) {
    self.discoverFeedService->RemoveFeedViewController(
        self.discoverFeedViewController);
  }
  self.discoverFeedWrapperViewController = nil;
  self.discoverFeedViewController = nil;
  self.feedMetricsRecorder = nil;

  [self.feedExpandedPref setObserver:nil];
  self.feedExpandedPref = nil;

  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _discoverFeedObserverBridge.reset();

  self.started = NO;
}

// Updates the visible property based on viewPresented and sceneInForeground
// properties.
// Sends metrics when NTP becomes invisible.
- (void)updateVisible {
  BOOL visible = self.viewPresented && self.sceneInForeground;
  if (visible == self.visible) {
    return;
  }
  self.visible = visible;
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    // Do not report metrics on incognito NTP.
    return;
  }
  if (visible) {
    self.didAppearTime = base::TimeTicks::Now();
    if ([self isFeedHeaderVisible]) {
      if ([self.feedExpandedPref value]) {
        ntp_home::RecordNTPImpression(ntp_home::REMOTE_SUGGESTIONS);
      } else {
        ntp_home::RecordNTPImpression(ntp_home::REMOTE_COLLAPSED);
      }
    } else {
      ntp_home::RecordNTPImpression(ntp_home::LOCAL_SUGGESTIONS);
    }
  } else {
    if (!self.didAppearTime.is_null()) {
      UmaHistogramMediumTimes("NewTabPage.TimeSpent",
                              base::TimeTicks::Now() - self.didAppearTime);
      self.didAppearTime = base::TimeTicks();
    }
  }
}

#pragma mark - ChromeCoordinatorHelpers

// Configures |self.ntpViewController| and sets it up as the main ViewController
// managed by this Coordinator.
- (void)configureNTPViewController {
  DCHECK(self.ntpViewController);

  if (IsContentSuggestionsUIViewControllerMigrationEnabled()) {
    self.ntpViewController.contentSuggestionsViewController =
        self.contentSuggestionsCoordinator.viewController;
  } else {
    self.ntpViewController.contentSuggestionsCollectionViewController =
        self.contentSuggestionsCoordinator
            .contentSuggestionsCollectionViewController;
  }

  self.ntpViewController.panGestureHandler = self.panGestureHandler;
  self.ntpViewController.feedVisible = [self isFeedVisible];

  self.discoverFeedWrapperViewController =
      [[DiscoverFeedWrapperViewController alloc]
                    initWithDelegate:self
          discoverFeedViewController:self.discoverFeedViewController];

  self.ntpViewController.feedTopSectionViewController =
      self.feedTopSectionCoordinator.viewController;

  self.headerSynchronizer = [[ContentSuggestionsHeaderSynchronizer alloc]
      initWithCollectionController:self.ntpViewController
                  headerController:[self headerController]];

  self.ntpViewController.discoverFeedWrapperViewController =
      self.discoverFeedWrapperViewController;
  self.ntpViewController.overscrollDelegate = self;
  self.ntpViewController.ntpContentDelegate = self;
  self.ntpViewController.identityDiscButton =
      [[self headerController] identityDiscButton];

  self.ntpViewController.headerController = [self headerController];

  [self configureMainViewControllerUsing:self.ntpViewController];
  self.ntpViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  self.ntpViewController.bubblePresenter = self.bubblePresenter;
}

// Configures the main ViewController managed by this Coordinator.
- (void)configureMainViewControllerUsing:
    (UIViewController*)containedViewController {
  [containedViewController
      willMoveToParentViewController:self.containerViewController];
  [self.containerViewController addChildViewController:containedViewController];
  [self.containerViewController.view addSubview:containedViewController.view];
  [containedViewController
      didMoveToParentViewController:self.containerViewController];

  containedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(containedViewController.view,
                     self.containerViewController.view);

  self.containedViewController = containedViewController;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  [self start];
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.containerViewController;
  }
}

- (id<ThumbStripSupporting>)thumbStripSupporting {
  return self.ntpViewController;
}

#pragma mark - Public Methods

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }
  self.contentSuggestionsCoordinator.webState = webState;
  self.ntpMediator.webState = webState;
  _webState = webState;
}

- (void)stopScrolling {
  if (!self.contentSuggestionsCoordinator) {
    return;
  }
  [self.ntpViewController stopScrolling];
}

- (UIEdgeInsets)contentInset {
  return [self.contentSuggestionsCoordinator contentInset];
}

- (CGPoint)contentOffset {
  return [self.contentSuggestionsCoordinator contentOffset];
}

- (void)willUpdateSnapshot {
  if (self.contentSuggestionsCoordinator.started) {
    [self.ntpViewController willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  if (self.discoverFeedViewController) {
    [self.ntpViewController focusFakebox];
  } else {
    [[self headerController] focusFakebox];
  }
}

- (void)reload {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  self.discoverFeedService->RefreshFeed();
  [self reloadContentSuggestions];
}

- (void)locationBarDidBecomeFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidBecomeFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidResignFirstResponder];
}

- (void)constrainDiscoverHeaderMenuButtonNamedGuide {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  NamedGuide* menuButtonGuide =
      [NamedGuide guideWithName:kDiscoverFeedHeaderMenuGuide
                           view:self.feedHeaderViewController.menuButton];

  menuButtonGuide.constrainedView = self.feedHeaderViewController.menuButton;
}

- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent {
  if (![self isFollowingFeedAvailable]) {
    return;
  }
  if ([self doesFollowingFeedHaveContent]) {
    [self.feedHeaderViewController
        updateFollowingSegmentDotForUnseenContent:hasUnseenContent];
  }
}

- (void)handleFeedModelDidEndUpdates:(FeedType)feedType {
  DCHECK(self.ntpViewController);
  if (!self.discoverFeedViewController) {
    return;
  }
  // When the visible feed has been updated, recalculate the minimum NTP height.
  if (![self isFollowingFeedAvailable] ||
      ([self isFollowingFeedAvailable] && feedType == self.selectedFeed)) {
    [self.ntpViewController updateFeedInsetsForMinimumHeight];
  }
}

- (void)ntpDidChangeVisibility:(BOOL)visible {
  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    if (visible && self.started) {
      [self.contentSuggestionsCoordinator configureStartSurfaceIfNeeded];
      if ([self isFollowingFeedAvailable]) {
        self.ntpViewController.shouldScrollIntoFeed = self.shouldScrollIntoFeed;
        self.shouldScrollIntoFeed = NO;
        // Reassign the sort type in case it changed in another tab.
        self.feedHeaderViewController.followingFeedSortType =
            (FollowingFeedSortType)self.prefService->GetInteger(
                prefs::kNTPFollowingFeedSortType);
        self.feedMetricsRecorder.feedControlDelegate = self;
        self.feedMetricsRecorder.followDelegate = self;
      }
    }
    if (!visible) {
      // Unfocus omnibox, to prevent it from lingering when it should be
      // dismissed (for example, when navigating away or when changing feed
      // visibility). Do this after the MVC classes are deallocated so no reset
      // animations are fired in response to this cancel.
      id<OmniboxCommands> omniboxCommandHandler = HandlerForProtocol(
          self.browser->GetCommandDispatcher(), OmniboxCommands);
      [omniboxCommandHandler cancelOmniboxEdit];
    }
  }
  self.viewPresented = visible;
  [self updateVisible];
}

#pragma mark - FeedControlDelegate

- (void)handleFeedSelected:(FeedType)feedType {
  DCHECK([self isFollowingFeedAvailable]);

  // Saves scroll position before changing feed.
  CGFloat scrollPosition = [self.ntpViewController scrollPosition];

  [self.feedMetricsRecorder recordFeedSelected:feedType];

  if (feedType == FeedTypeFollowing) {
    // Clears dot and notifies service that the Following feed content has
    // been seen.
    [self.feedHeaderViewController
        updateFollowingSegmentDotForUnseenContent:NO];
    self.discoverFeedService->SetFollowingFeedContentSeen();
  }

  self.selectedFeed = feedType;
  [self updateNTPForFeed];
  [self updateFeedLayout];

  [self.ntpViewController updateFeedInsetsForMinimumHeight];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.ntpViewController setContentOffsetToTopOfFeed:scrollPosition];
}

- (void)handleSortTypeForFollowingFeed:(FollowingFeedSortType)sortType {
  DCHECK([self isFollowingFeedAvailable]);
  self.prefService->SetInteger(prefs::kNTPFollowingFeedSortType, sortType);
  self.discoverFeedService->SetFollowingFeedSortType(sortType);
  self.feedHeaderViewController.followingFeedSortType = sortType;

  // Changing the sort type affects the scroll position, so update the feed
  // layout.
  [self updateFeedLayout];
}

- (BOOL)shouldFeedBeVisible {
  return [self isFeedHeaderVisible] && [self.feedExpandedPref value] &&
         !IsFeedAblationEnabled();
}

- (BOOL)isFollowingFeedAvailable {
  return IsWebChannelsEnabled() &&
         self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

#pragma mark - NewTabPageFollowDelegate

- (NSUInteger)followedPublisherCount {
  return [ios::GetChromeBrowserProvider()
              .GetFollowProvider()
              ->GetFollowedWebChannels() count];
}

- (BOOL)doesFollowingFeedHaveContent {
  return ios::GetChromeBrowserProvider()
      .GetFollowProvider()
      ->DoesFollowingFeedHaveContent();
}

#pragma mark - FeedMenuCommands

- (void)openFeedMenu {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;

  self.alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.ntpViewController
                         browser:self.browser
                           title:nil
                         message:nil
                            rect:self.feedHeaderViewController.menuButton.frame
                            view:self.feedHeaderViewController.view];
  __weak NewTabPageCoordinator* weakSelf = self;

  // Item for toggling the feed on/off.
  if ([self.feedExpandedPref value]) {
    [self.alertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM)
                  action:^{
                    [weakSelf setFeedVisibleFromHeader:NO];
                  }
                   style:UIAlertActionStyleDestructive];
  } else {
    [self.alertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM)
                  action:^{
                    [weakSelf setFeedVisibleFromHeader:YES];
                  }
                   style:UIAlertActionStyleDefault];
  }

  // Items for signed-in users.
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    if ([self isFollowingFeedAvailable]) {
      [self.alertCoordinator
          addItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM)
                    action:^{
                      [weakSelf handleFeedManageTapped];
                    }
                     style:UIAlertActionStyleDefault];
    } else {
      [self.alertCoordinator
          addItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ACTIVITY_ITEM)
                    action:^{
                      [weakSelf.ntpMediator handleFeedManageActivityTapped];
                    }
                     style:UIAlertActionStyleDefault];

      [self.alertCoordinator
          addItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_MENU_MANAGE_INTERESTS_ITEM)
                    action:^{
                      [weakSelf.ntpMediator handleFeedManageInterestsTapped];
                    }
                     style:UIAlertActionStyleDefault];
    }
  }

  // Items for all users.
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM)
                action:^{
                  [weakSelf.ntpMediator handleFeedLearnMoreTapped];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - ContentSuggestionsHeaderCommands

- (void)updateForHeaderSizeChange {
  [self updateFeedLayout];
}

#pragma mark - NewTabPageDelegate

- (void)updateFeedLayout {
  // If this coordinator has not finished [self start], the below will start
  // viewDidLoad before the UI is ready, failing DCHECKS.
  if (!self.started) {
    return;
  }
  [self.containedViewController.view setNeedsLayout];
  [self.containedViewController.view layoutIfNeeded];
  [self.ntpViewController updateNTPLayout];
}

- (void)setContentOffsetToTop {
  [self.ntpViewController setContentOffsetToTop];
}

- (BOOL)isGoogleDefaultSearchEngine {
  DCHECK(self.templateURLService);
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(self.templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  return isGoogleDefaultSearchProvider;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  DCHECK(IsContentSuggestionsHeaderMigrationEnabled());
  if (base::FeatureList::IsEnabled(kEnableFREUIModuleIOS)) {
    if (previousInitStage == InitStageFirstRun) {
      [self headerController].focusOmniboxWhenViewAppears = YES;
      [[self headerController] focusAccessibilityOnOmnibox];

      [appState removeObserver:self];
    }
  }
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [[self headerController] logoAnimationControllerOwner];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  self.sceneInForeground = level >= SceneActivationLevelForegroundInactive;
  [self updateVisible];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self handleFeedVisibilityDidChange];
}

#pragma mark - DiscoverFeedDelegate

- (void)contentSuggestionsWasUpdated {
  [self updateFeedLayout];
}

- (void)returnToRecentTabWasAdded {
  [self updateFeedLayout];
  [self setContentOffsetToTop];
}

#pragma mark - DiscoverFeedObserverBridge

- (void)discoverFeedModelWasCreated {
  if (self.ntpViewController.viewDidAppear) {
    [self updateNTPForFeed];

    if (IsWebChannelsEnabled()) {
      [self.feedHeaderViewController updateForFollowingFeedVisibilityChanged];
      [self.ntpViewController updateNTPLayout];
      [self updateFeedLayout];
      [self.ntpViewController setContentOffsetToTop];
    }
  } else {
    // If the NTP hasn't been completely configured (which happens by the time
    // its view has appeared) just refresh the feed instead of updating the
    // whole NTP.
    self.discoverFeedService->RefreshFeed();
  }
}

#pragma mark - DiscoverFeedPreviewDelegate

- (UIViewController*)discoverFeedPreviewWithURL:(const GURL)URL {
  std::string referrerURL = base::GetFieldTrialParamValueByFeature(
      kEnableDiscoverFeedPreview, kDiscoverReferrerParameter);
  if (referrerURL.empty()) {
    referrerURL = kDefaultDiscoverReferrer;
  }

  self.linkPreviewCoordinator =
      [[LinkPreviewCoordinator alloc] initWithBrowser:self.browser URL:URL];
  self.linkPreviewCoordinator.referrer =
      web::Referrer(GURL(referrerURL), web::ReferrerPolicyDefault);
  [self.linkPreviewCoordinator start];
  return [self.linkPreviewCoordinator linkPreviewViewController];
}

- (void)didTapDiscoverFeedPreview {
  DCHECK(self.linkPreviewCoordinator);
  [self.linkPreviewCoordinator handlePreviewAction];
  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action {
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  id<ApplicationCommands, BrowserCommands, OmniboxCommands, SnackbarCommands>
      handler = static_cast<id<ApplicationCommands, BrowserCommands,
                               OmniboxCommands, SnackbarCommands>>(
          self.browser->GetCommandDispatcher());
  switch (action) {
    case OverscrollAction::NEW_TAB: {
      [handler openURLInNewTab:[OpenNewTabCommand command]];
    } break;
    case OverscrollAction::CLOSE_TAB: {
      [handler closeCurrentTab];
      base::RecordAction(base::UserMetricsAction("OverscrollActionCloseTab"));
    } break;
    case OverscrollAction::REFRESH:
      [self reload];
      break;
    case OverscrollAction::NONE:
      NOTREACHED();
      break;
  }
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return YES;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return
      [[[self headerController] toolBarView] snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.discoverFeedWrapperViewController.view;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [self.ntpViewController heightAboveFeed];
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat height = [[self headerController] toolBarView].bounds.size.height;
  CGFloat topInset =
      self.discoverFeedWrapperViewController.view.safeAreaInsets.top;
  return height + topInset;
}

- (CGFloat)initialContentOffsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return -[self headerInsetForOverscrollActionsController:controller];
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // Fullscreen isn't supported here.
  return nullptr;
}

#pragma mark - NewTabPageContentDelegate

- (void)reloadContentSuggestions {
  if (IsContentSuggestionsHeaderMigrationEnabled() &&
      IsContentSuggestionsUIViewControllerMigrationEnabled()) {
    // No need to reload ContentSuggestions since the mediator receives all
    // model state changes and immediately updates the consumer with the new
    // state.
    return;
  }
  [self.contentSuggestionsCoordinator reload];
}

- (BOOL)isContentHeaderSticky {
  return [self isFollowingFeedAvailable];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (!self.started) {
    return;
  }
  if (preferenceName == prefs::kArticlesForYouEnabled ||
      preferenceName == prefs::kNTPContentSuggestionsEnabled) {
    [self updateNTPForFeed];
  }
  if (preferenceName ==
      DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
    [self defaultSearchEngineDidChange];
  }
}

#pragma mark - FeedManagementNavigationDelegate

- (void)handleNavigateToActivity {
  [self.ntpMediator handleFeedManageActivityTapped];
}

- (void)handleNavigateToInterests {
  [self.ntpMediator handleFeedManageInterestsTapped];
}

- (void)handleNavigateToHidden {
  [self.ntpMediator handleFeedManageHiddenTapped];
}

- (void)handleNavigateToFollowedURL:(const GURL&)url {
  [self.ntpMediator handleVisitSiteFromFollowManagementList:url];
}

#pragma mark - Private

// Updates the NTP to take into account a new feed, or a change in feed
// visibility.
- (void)updateNTPForFeed {
  DCHECK(self.ntpViewController);

  if (!self.started) {
    return;
  }

  [self.ntpViewController resetViewHierarchy];

  if (self.discoverFeedViewController) {
    self.discoverFeedService->RemoveFeedViewController(
        self.discoverFeedViewController);
  }

  self.ntpViewController.discoverFeedWrapperViewController = nil;
  self.discoverFeedWrapperViewController = nil;
  self.discoverFeedViewController = nil;

  // Fetches feed header and conditionally fetches feed. Feed can only be
  // visible if feed header is visible.
  if ([self isFeedHeaderVisible]) {
    [self configureFeedAndHeader];
  } else {
    self.ntpViewController.feedHeaderViewController = nil;
    self.feedHeaderViewController = nil;
  }

  self.ntpViewController.feedVisible = [self isFeedVisible];

  self.discoverFeedWrapperViewController =
      [[DiscoverFeedWrapperViewController alloc]
                    initWithDelegate:self
          discoverFeedViewController:self.discoverFeedViewController];

  self.ntpViewController.discoverFeedWrapperViewController =
      self.discoverFeedWrapperViewController;

  [self.ntpViewController layoutContentInParentCollectionView];
}

// Creates and configures the feed and feed header based on user prefs.
- (void)configureFeedAndHeader {
  DCHECK([self isFeedHeaderVisible]);

  self.ntpViewController.feedHeaderViewController =
      self.feedHeaderViewController;

  // Requests feeds here if the correct flags and prefs are enabled.
  if ([self shouldFeedBeVisible]) {
    FeedModelConfiguration* discoverFeedConfiguration =
        [FeedModelConfiguration discoverFeedModelConfiguration];
    self.discoverFeedService->CreateFeedModel(discoverFeedConfiguration);

    if ([self isFollowingFeedAvailable]) {
      FeedModelConfiguration* followingFeedConfiguration =
          [FeedModelConfiguration
              followingModelConfigurationWithSortType:
                  (FollowingFeedSortType)self.prefService->GetInteger(
                      prefs::kNTPFollowingFeedSortType)];
      self.discoverFeedService->CreateFeedModel(followingFeedConfiguration);

      // TODO(crbug.com/1277504): Use unique property for Following feed.
      switch (self.selectedFeed) {
        case FeedTypeDiscover:
          self.discoverFeedViewController = [self discoverFeed];
          break;
        case FeedTypeFollowing:
          self.discoverFeedViewController = [self followingFeed];
          break;
      }
    } else {
      self.discoverFeedViewController = [self discoverFeed];
    }
  }
}

// TODO(crbug.com/1285378): Remove this after
// kContentSuggestionsHeaderMigrationEnabled is launched.
- (ContentSuggestionsHeaderViewController*)headerController {
  if (IsContentSuggestionsHeaderMigrationEnabled()) {
    return _headerController;
  } else {
    return self.contentSuggestionsCoordinator.headerController;
  }
}

// Feed header is always visible unless it is disabled from the Chrome settings
// menu, or by an enterprise policy.
- (BOOL)isFeedHeaderVisible {
  return self.prefService->GetBoolean(prefs::kArticlesForYouEnabled) &&
         self.prefService->GetBoolean(prefs::kNTPContentSuggestionsEnabled) &&
         !IsFeedAblationEnabled();
}

// Returns |YES| if the feed is currently visible on the NTP.
- (BOOL)isFeedVisible {
  return [self shouldFeedBeVisible] && self.discoverFeedViewController;
}

// Creates, configures and returns a Discover feed view controller.
- (UIViewController*)discoverFeed {
  if (tests_hook::DisableDiscoverFeed()) {
    return nil;
  }

  UIViewController* discoverFeed =
      self.discoverFeedService->NewDiscoverFeedViewControllerWithConfiguration(
          [self feedViewControllerConfiguration]);
  return discoverFeed;
}

// Creates, configures and returns a Following feed view controller.
- (UIViewController*)followingFeed {
  if (tests_hook::DisableDiscoverFeed()) {
    return nil;
  }

  UIViewController* followingFeed =
      self.discoverFeedService->NewFollowingFeedViewControllerWithConfiguration(
          [self feedViewControllerConfiguration]);
  return followingFeed;
}

// Creates, configures and returns a feed view controller configuration.
- (DiscoverFeedViewControllerConfiguration*)feedViewControllerConfiguration {
  DiscoverFeedViewControllerConfiguration* viewControllerConfig =
      [[DiscoverFeedViewControllerConfiguration alloc] init];
  viewControllerConfig.browser = self.browser;
  viewControllerConfig.scrollDelegate = self.ntpViewController;
  viewControllerConfig.previewDelegate = self;

  return viewControllerConfig;
}

// Handles how the NTP reacts when the default search engine is changed.
- (void)defaultSearchEngineDidChange {
  [self.feedHeaderViewController updateForDefaultSearchEngineChanged];
  [self.ntpViewController updateNTPLayout];
  [self updateFeedLayout];
  [self.ntpViewController setContentOffsetToTop];
}

// Toggles feed visibility between hidden or expanded using the feed header
// menu. A hidden feed will continue to show the header, with a modified label.
// TODO(crbug.com/1304382): Modify this comment when Web Channels is launched.
- (void)setFeedVisibleFromHeader:(BOOL)visible {
  [self.feedExpandedPref setValue:visible];
  [self.feedMetricsRecorder recordDiscoverFeedVisibilityChanged:visible];
  [self handleFeedVisibilityDidChange];
}

// Configures and returns the NTP mediator.
- (NTPHomeMediator*)createNTPMediator {
  NTPHomeMediator* ntpMediator = [[NTPHomeMediator alloc]
             initWithWebState:self.webState
           templateURLService:self.templateURLService
                    URLLoader:UrlLoadingBrowserAgent::FromBrowser(self.browser)
                  authService:self.authService
              identityManager:IdentityManagerFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
        accountManagerService:ChromeAccountManagerServiceFactory::
                                  GetForBrowserState(
                                      self.browser->GetBrowserState())
                   logoVendor:ios::provider::CreateLogoVendor(self.browser,
                                                              self.webState)
      voiceSearchAvailability:&_voiceSearchAvailability];
  ntpMediator.browser = self.browser;
  ntpMediator.ntpViewController = self.ntpViewController;
  ntpMediator.headerCollectionInteractionHandler = self.headerSynchronizer;
  ntpMediator.feedControlDelegate = self;
  return ntpMediator;
}

// Configures and returns a content suggestions coordinator.
- (ContentSuggestionsCoordinator*)createContentSuggestionsCoordinator {
  ContentSuggestionsCoordinator* contentSuggestionsCoordinator =
      [[ContentSuggestionsCoordinator alloc]
          initWithBaseViewController:nil
                             browser:self.browser];
  contentSuggestionsCoordinator.webState = self.webState;
  if (!IsContentSuggestionsHeaderMigrationEnabled()) {
    contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
  }
  contentSuggestionsCoordinator.ntpMediator = self.ntpMediator;
  contentSuggestionsCoordinator.ntpDelegate = self;
  contentSuggestionsCoordinator.discoverFeedDelegate = self;
  [contentSuggestionsCoordinator start];
  if (!IsContentSuggestionsHeaderMigrationEnabled()) {
    contentSuggestionsCoordinator.headerController.baseViewController =
        self.baseViewController;
  }
  return contentSuggestionsCoordinator;
}

// Configures and returns the feed top section coordinator.
- (FeedTopSectionCoordinator*)createFeedTopSectionCoordinator {
  DCHECK(self.ntpViewController);
  FeedTopSectionCoordinator* feedTopSectionCoordinator =
      [[FeedTopSectionCoordinator alloc]
          initWithBaseViewController:self.ntpViewController
                             browser:self.browser];
  [feedTopSectionCoordinator start];
  return feedTopSectionCoordinator;
}

- (void)handleFeedManageTapped {
  [self.feedMetricsRecorder recordHeaderMenuManageTapped];
  [self.feedManagementCoordinator stop];
  self.feedManagementCoordinator = nil;

  self.feedManagementCoordinator = [[FeedManagementCoordinator alloc]
      initWithBaseViewController:self.ntpViewController
                         browser:self.browser];
  self.feedManagementCoordinator.navigationDelegate = self;
  self.feedManagementCoordinator.feedMetricsRecorder = self.feedMetricsRecorder;
  [self.feedManagementCoordinator start];
}

// Handles how the NTP should react when the feed visbility preference is
// changed.
- (void)handleFeedVisibilityDidChange {
  [self updateNTPForFeed];
  [self.feedHeaderViewController updateForFeedVisibilityChanged];
}

#pragma mark - Getters

- (FeedHeaderViewController*)feedHeaderViewController {
  DCHECK(!self.browser->GetBrowserState()->IsOffTheRecord());
  if (!_feedHeaderViewController) {
    // Only show the dot if the user follows available publishers.
    BOOL followingSegmentDotVisible =
        [self doesFollowingFeedHaveContent] &&
        self.discoverFeedService->GetFollowingFeedHasUnseenContent() &&
        self.selectedFeed != FeedTypeFollowing;
    _feedHeaderViewController = [[FeedHeaderViewController alloc]
        initWithFollowingFeedSortType:(FollowingFeedSortType)
                                          self.prefService->GetInteger(
                                              prefs::kNTPFollowingFeedSortType)
           followingSegmentDotVisible:followingSegmentDotVisible];
    _feedHeaderViewController.feedControlDelegate = self;
    _feedHeaderViewController.ntpDelegate = self;
    [_feedHeaderViewController.menuButton
               addTarget:self
                  action:@selector(openFeedMenu)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return _feedHeaderViewController;
}

#pragma mark - DiscoverFeedWrapperViewControllerDelegate

- (void)updateTheme {
  self.discoverFeedService->UpdateTheme();
}

@end
