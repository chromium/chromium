// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/feed/core/v2/public/ios/pref_names.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/default_search_manager.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#include "ios/chrome/app/tests_hook.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/pref_names.h"
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
#import "ios/chrome/browser/ui/ntp/feed_menu_commands.h"
#import "ios/chrome/browser/ui/ntp/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/voice/voice_search_availability.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_observer_bridge.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_provider.h"
#import "ios/public/provider/chrome/browser/discover_feed/discover_feed_view_controller_configuration.h"
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

// Kill switch guarding a fix an NTP/discover memory leak fix. Behind a feature
// flag so we can validate the impact, as well as safety for a stable respin.
const base::Feature kUpdateNTPForFeedFix{"UpdateNTPForFeedFix",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

@interface NewTabPageCoordinator () <BooleanObserver,
                                     DiscoverFeedDelegate,
                                     DiscoverFeedObserverBridgeDelegate,
                                     DiscoverFeedPreviewDelegate,
                                     FeedControlDelegate,
                                     FeedMenuCommands,
                                     NewTabPageCommands,
                                     NewTabPageContentDelegate,
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
  std::unique_ptr<DiscoverFeedObserverBridge>
      _discoverFeedProviderObserverBridge;
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

// The coordinator contained ViewController. It can be either a
// NewTabPageViewController (When the Discover Feed is being shown) or a
// ContentSuggestionsViewController (When the Discover Feed is hidden or when
// the non refactored NTP is being used.)
// TODO(crbug.com/1114792): Update this comment when the NTP refactors launches.
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

// Metrics recorder for actions relating to the feed.
@property(nonatomic, strong) FeedMetricsRecorder* feedMetricsRecorder;

// Currently selected feed.
@property(nonatomic, assign) FeedType selectedFeed;

@end

@implementation NewTabPageCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _containerViewController = [[UIViewController alloc] init];

    _prefService =
        ChromeBrowserState::FromBrowserState(browser->GetBrowserState())
            ->GetPrefs();
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
    _feedExpandedPref = [[PrefBackedBoolean alloc]
        initWithPrefService:_prefService
                   prefName:feed::prefs::kArticlesListVisible];
    [_feedExpandedPref setObserver:self];
    _discoverFeedProviderObserverBridge =
        std::make_unique<DiscoverFeedObserverBridge>(self);

    // TODO(crbug.com/1277974): Make sure that we always want the Discover feed
    // as default.
    _selectedFeed = FeedType::kDiscoverFeed;
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

  self.ntpViewController = [[NewTabPageViewController alloc] init];

  self.ntpMediator = [self createNTPMediator];

  // Creating the DiscoverFeedService will start the Discover feed.
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  self.feedMetricsRecorder = discoverFeedService->GetFeedMetricsRecorder();

  self.contentSuggestionsCoordinator =
      [self createContentSuggestionsCoordinator];

  // Fetches feed header and conditionally fetches feed. Feed can only be
  // visible if feed header is visible.
  if ([self isFeedHeaderVisible]) {
    self.feedHeaderViewController = [[FeedHeaderViewController alloc]
        initWithSelectedFeed:self.selectedFeed];
    self.feedHeaderViewController.feedControlDelegate = self;

    [self updateFeedHeaderLabelText:self.feedHeaderViewController];

    // Requests a Discover feed here if the correct flags and prefs are enabled.
    if ([self shouldFeedBeVisible]) {
      if (IsWebChannelsEnabled()) {
        // TODO(crbug.com/1277504): Use unique property for Following feed.
        switch (self.selectedFeed) {
          case FeedType::kDiscoverFeed:
            self.discoverFeedViewController = [self discoverFeed];
            break;
          case FeedType::kFollowingFeed:
            self.discoverFeedViewController = [self followingFeed];
            break;
        }
      } else {
        self.discoverFeedViewController = [self discoverFeed];
      }
    }
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

  // Unfocus omnibox, to prevent it from lingering when it should be dismissed
  // (for example, when navigating away or when changing feed visibility).
  id<OmniboxCommands> omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandHandler cancelOmniboxEdit];

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState removeObserver:self];

  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.incognitoViewController = nil;
  self.ntpViewController = nil;
  self.feedHeaderViewController = nil;
  self.alertCoordinator = nil;
  self.authService = nil;
  self.templateURLService = nil;

  [self.ntpMediator shutdown];
  self.ntpMediator = nil;

  ios::GetChromeBrowserProvider()
      .GetDiscoverFeedProvider()
      ->RemoveFeedViewController(self.discoverFeedViewController);
  self.discoverFeedWrapperViewController = nil;
  self.discoverFeedViewController = nil;
  self.feedMetricsRecorder = nil;

  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];

  self.started = NO;
}

- (void)disconnect {
  // TODO(crbug.com/1200303): Move this to stop once we stop starting/stopping
  // the Coordinator when turning the feed on/off.
  [self.feedExpandedPref setObserver:nil];
  self.feedExpandedPref = nil;

  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _discoverFeedProviderObserverBridge.reset();
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
  self.ntpViewController.contentSuggestionsViewController =
      self.contentSuggestionsCoordinator.viewController;
  self.ntpViewController.panGestureHandler = self.panGestureHandler;
  self.ntpViewController.feedVisible =
      [self shouldFeedBeVisible] && self.discoverFeedViewController;

  self.discoverFeedWrapperViewController =
      [[DiscoverFeedWrapperViewController alloc]
          initWithDiscoverFeedViewController:self.discoverFeedViewController];

  self.headerSynchronizer = [[ContentSuggestionsHeaderSynchronizer alloc]
      initWithCollectionController:self.ntpViewController
                  headerController:self.contentSuggestionsCoordinator
                                       .headerController];

  self.ntpViewController.discoverFeedWrapperViewController =
      self.discoverFeedWrapperViewController;
  self.ntpViewController.overscrollDelegate = self;
  self.ntpViewController.ntpContentDelegate = self;
  self.ntpViewController.feedMenuHandler = self;
  self.ntpViewController.identityDiscButton =
      [self.contentSuggestionsCoordinator.headerController identityDiscButton];

  self.ntpViewController.headerController =
      self.contentSuggestionsCoordinator.headerController;

  self.ntpViewController.feedHeaderViewController =
      self.feedHeaderViewController;

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
  self.ntpMediator.webState = webState;
  _webState = webState;
}

- (void)dismissModals {
  [self.contentSuggestionsCoordinator dismissModals];
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
    [self.contentSuggestionsCoordinator.headerController focusFakebox];
  }
}

- (void)reload {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  ios::GetChromeBrowserProvider().GetDiscoverFeedProvider()->RefreshFeed();
  [self reloadContentSuggestions];
}

- (void)locationBarDidBecomeFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidBecomeFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidResignFirstResponder];
}

- (void)constrainDiscoverHeaderMenuButtonNamedGuide {
  NamedGuide* menuButtonGuide =
      [NamedGuide guideWithName:kDiscoverFeedHeaderMenuGuide
                           view:self.feedHeaderViewController.menuButton];

  menuButtonGuide.constrainedView = self.feedHeaderViewController.menuButton;
}

- (void)ntpDidChangeVisibility:(BOOL)visible {
  if (visible) {
    [self.contentSuggestionsCoordinator configureStartSurfaceIfNeeded];
  }
  self.viewPresented = visible;
  [self updateVisible];
}

#pragma mark - FeedControlDelegate

- (void)handleFeedSelected:(FeedType)feedType {
  self.selectedFeed = feedType;
  [self updateNTPForFeed];
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
                    [weakSelf updateNTPForFeed];
                  }
                   style:UIAlertActionStyleDestructive];
  } else {
    [self.alertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM)
                  action:^{
                    [weakSelf setFeedVisibleFromHeader:YES];
                    [weakSelf updateNTPForFeed];
                  }
                   style:UIAlertActionStyleDefault];
  }

  // Items for signed-in users.
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
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

#pragma mark - NewTabPageCommands

- (void)updateNTPForFeed {
  static bool update_ntp_for_feed_fix =
      base::FeatureList::IsEnabled(kUpdateNTPForFeedFix);
  if (update_ntp_for_feed_fix && !self.started) {
    return;
  }

  [self stop];
  [self start];
  [self updateDiscoverFeedLayout];

  [self.containerViewController.view setNeedsLayout];
  [self.containerViewController.view layoutIfNeeded];
}

- (void)updateDiscoverFeedLayout {
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

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.contentSuggestionsCoordinator
              .headerController logoAnimationControllerOwner];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  self.sceneInForeground = level >= SceneActivationLevelForegroundInactive;
  [self updateVisible];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self updateNTPForFeed];
}

#pragma mark - DiscoverFeedDelegate

- (void)contentSuggestionsWasUpdated {
  [self updateDiscoverFeedLayout];
  [self setContentOffsetToTop];
}

- (void)returnToRecentTabWasAdded {
  [self updateDiscoverFeedLayout];
  [self setContentOffsetToTop];
}

#pragma mark - DiscoverFeedObserverBridge

- (void)onDiscoverFeedModelRecreated {
  [self updateNTPForFeed];
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
  return [[self.contentSuggestionsCoordinator.headerController toolBarView]
      snapshotViewAfterScreenUpdates:NO];
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
  CGFloat height =
      [self.contentSuggestionsCoordinator.headerController toolBarView]
          .bounds.size.height;
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
  [self.contentSuggestionsCoordinator reload];
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

#pragma mark - Private

// Feed header is always visible unless it is disabled from the Chrome settings
// menu, or by an enterprise policy.
- (BOOL)isFeedHeaderVisible {
  return self.prefService->GetBoolean(prefs::kArticlesForYouEnabled) &&
         self.prefService->GetBoolean(prefs::kNTPContentSuggestionsEnabled);
}

// Determines whether the feed should be fetched based on the user prefs.
- (BOOL)shouldFeedBeVisible {
  return [self isFeedHeaderVisible] && [self.feedExpandedPref value];
}

// Creates, configures and returns a Discover feed view controller.
- (UIViewController*)discoverFeed {
  if (tests_hook::DisableDiscoverFeed())
    return nil;

  UIViewController* discoverFeed =
      ios::GetChromeBrowserProvider()
          .GetDiscoverFeedProvider()
          ->NewDiscoverFeedViewControllerWithConfiguration(
              [self feedViewControllerConfiguration]);
  return discoverFeed;
}

// Creates, configures and returns a Following feed view controller.
- (UIViewController*)followingFeed {
  if (tests_hook::DisableDiscoverFeed())
    return nil;

  UIViewController* followingFeed =
      ios::GetChromeBrowserProvider()
          .GetDiscoverFeedProvider()
          ->NewFollowingFeedViewControllerWithConfiguration(
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

// Handles how the NTP should react when the default search engine setting is
// changed.
- (void)defaultSearchEngineDidChange {
  [self updateFeedHeaderLabelText:self.feedHeaderViewController];
  BOOL isScrolledToTop = [self.ntpViewController isNTPScrolledToTop];
  [self updateDiscoverFeedLayout];
  // Ensures doodle is visible if content suggestions height changes when
  // scrolled to top. Otherwise, maintain scroll position.
  if (isScrolledToTop) {
    [self.ntpViewController setContentOffsetToTop];
  }
}

// Toggles feed visibility between hidden or expanded using the feed header
// menu. A hidden feed will continue to show the header, with a modified label.
- (void)setFeedVisibleFromHeader:(BOOL)visible {
  [self.feedExpandedPref setValue:visible];
  [self.feedMetricsRecorder recordDiscoverFeedVisibilityChanged:visible];
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
  ntpMediator.NTPMetrics = [[NTPHomeMetrics alloc]
      initWithBrowserState:self.browser->GetBrowserState()];
  ntpMediator.NTPMetrics.webState = self.webState;
  return ntpMediator;
}

// Configures and returns a content suggestions coordinator.
- (ContentSuggestionsCoordinator*)createContentSuggestionsCoordinator {
  ContentSuggestionsCoordinator* contentSuggestionsCoordinator =
      [[ContentSuggestionsCoordinator alloc]
          initWithBaseViewController:nil
                             browser:self.browser];
  contentSuggestionsCoordinator.webState = self.webState;
  contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
  contentSuggestionsCoordinator.ntpMediator = self.ntpMediator;
  contentSuggestionsCoordinator.ntpCommandHandler = self;
  contentSuggestionsCoordinator.discoverFeedDelegate = self;
  contentSuggestionsCoordinator.feedMetricsRecorder = self.feedMetricsRecorder;
  [contentSuggestionsCoordinator start];
  contentSuggestionsCoordinator.headerController.baseViewController =
      self.baseViewController;
  return contentSuggestionsCoordinator;
}

// Sets a header's text based on feed visibility and default search engine
// prefs.
- (void)updateFeedHeaderLabelText:(FeedHeaderViewController*)feedHeader {
  if (!self.templateURLService) {
    return;
  }
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(self.templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  NSString* feedHeaderTitleText =
      isGoogleDefaultSearchProvider
          ? l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE)
          : l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_TITLE_NON_DSE);
  feedHeaderTitleText =
      [self shouldFeedBeVisible]
          ? feedHeaderTitleText
          : [NSString
                stringWithFormat:@"%@ – %@", feedHeaderTitleText,
                                 l10n_util::GetNSString(
                                     IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
  [feedHeader setTitleText:feedHeaderTitleText];
}

@end
