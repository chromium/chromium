// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/feed/feed_feature_list.h"
#import "components/policy/policy_constants.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/account_menu/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_coordinator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_delegate.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_mediator.h"
#import "ios/chrome/browser/context_menu/ui_bundled/link_preview/link_preview_coordinator.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_observer_bridge.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_observer.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/discover_feed/model/feed_model_configuration.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/follow/model/followed_web_site.h"
#import "ios/chrome/browser/follow/model/followed_web_site_state.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_coordinator.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/discover_feed_preview_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_sign_in_promo_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_top_section/feed_top_section_coordinator.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/home_start_data_source.h"
#import "ios/chrome/browser/ntp/ui_bundled/incognito/incognito_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/logo_vendor.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_component_factory_protocol.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator+Testing.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_follow_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_view_controller.h"
#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_coordinator.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_params.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/authentication_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/supervised_user/model/family_link_user_capabilities_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/fakebox_focuser.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/coordinator/tab_group_indicator_coordinator.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface NewTabPageCoordinator () <AccountMenuCoordinatorDelegate,
                                     AuthenticationServiceObserving,
                                     ContentSuggestionsDelegate,
                                     DiscoverFeedObserverBridgeDelegate,
                                     DiscoverFeedPreviewDelegate,
                                     DiscoverFeedVisibilityObserver,
                                     FeedControlDelegate,
                                     FeedSignInPromoDelegate,
                                     FeedWrapperViewControllerDelegate,
                                     HomeCustomizationDelegate,
                                     HomeStartDataSource,
                                     IdentityManagerObserverBridgeDelegate,
                                     NewTabPageContentDelegate,
                                     NewTabPageDelegate,
                                     NewTabPageFollowDelegate,
                                     NewTabPageHeaderCommands,
                                     NewTabPageActionsDelegate,
                                     OverscrollActionsControllerDelegate,
                                     ProfileStateObserver,
                                     SceneStateObserver,
                                     FamilyLinkUserCapabilitiesObserving> {
  // Observes changes in the IdentityManager.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;

  // Observes changes in the DiscoverFeed.
  std::unique_ptr<DiscoverFeedObserverBridge> _discoverFeedObserverBridge;

  // Observer for auth service status changes.
  std::unique_ptr<AuthenticationServiceObserverBridge>
      _authServiceObserverBridge;

  // Observer to track changes to supervision-related capabilities.
  std::unique_ptr<supervised_user::FamilyLinkUserCapabilitiesObserverBridge>
      _familyLinkUserCapabilitiesObserverBridge;

  BubbleViewControllerPresenter* _fakeboxLensIconBubblePresenter;
}

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* NTPViewController;

// Mediator owned by this coordinator.
@property(nonatomic, strong) NewTabPageMediator* NTPMediator;

// View controller wrapping the feed.
@property(nonatomic, strong)
    FeedWrapperViewController* feedWrapperViewController;

// View controller for the incognito NTP.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

// The timetick of the last time the NTP was displayed.
@property(nonatomic, assign) base::TimeTicks didAppearTime;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
// Redefined to readwrite.
@property(nonatomic, assign, readwrite) BOOL visible;

// The ViewController displayed by this Coordinator. This is the returned
// ViewController and will contain the `containedViewController` (Which can
// change depending on Feed visibility).
@property(nonatomic, strong) UIViewController* containerViewController;

// The coordinator contained ViewController.
@property(nonatomic, strong) UIViewController* containedViewController;

// PrefService used by this Coordinator.
@property(nonatomic, assign) PrefService* prefService;

// The view controller representing the selected feed, such as the Discover or
// Following feed.
@property(nonatomic, weak) UIViewController* feedViewController;

// The Coordinator to display previews for Discover feed websites. It also
// handles the actions related to them.
@property(nonatomic, strong) LinkPreviewCoordinator* linkPreviewCoordinator;

// The view controller representing the NTP feed header.
@property(nonatomic, strong) FeedHeaderViewController* feedHeaderViewController;

// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;

// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// DiscoverFeed Service to display the Feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;

// Metrics recorder for actions relating to the feed.
@property(nonatomic, weak) FeedMetricsRecorder* feedMetricsRecorder;

// The header view controller containing the fake omnibox and logo.
@property(nonatomic, strong)
    NewTabPageHeaderViewController* headerViewController;

// Coordinator for Feed top section.
@property(nonatomic, strong)
    FeedTopSectionCoordinator* feedTopSectionCoordinator;

// Currently selected feed. Redefined to readwrite.
@property(nonatomic, assign, readwrite) FeedType selectedFeed;

// The Webstate associated with this coordinator.
@property(nonatomic, assign) web::WebState* webState;

// Returns `YES` if the coordinator is started.
@property(nonatomic, assign) BOOL started;

// Contains a factory which can generate NTP components which are initialized
// on `start`.
@property(nonatomic, strong) id<NewTabPageComponentFactoryProtocol>
    componentFactory;

// Recorder for new tab page metrics.
@property(nonatomic, strong) NewTabPageMetricsRecorder* NTPMetricsRecorder;

// Logo vendor to display the doodle on the NTP.
@property(nonatomic, strong) id<LogoVendor> logoVendor;

@end

@implementation NewTabPageCoordinator {
  // Coordinator in charge of handling sharing use cases.
  SharingCoordinator* _sharingCoordinator;
  // Coordinator for presenting the Home customization menu.
  HomeCustomizationCoordinator* _customizationCoordinator;
  // Coordinator for the tab group indicator.
  TabGroupIndicatorCoordinator* _tabGroupIndicatorCoordinator;
  // Indicates whether the fakebox was tapped as part of an omnibox focus event.
  BOOL _fakeboxTapped;
  // The account menu coordinator.
  AccountMenuCoordinator* _accountMenuCoordinator;
  // The sign in and history sync coordinator displayed on top of the NTP.
  SigninCoordinator* _signinCoordinator;
}

// Synthesize NewTabPageConfiguring properties.
@synthesize shouldScrollIntoFeed = _shouldScrollIntoFeed;
@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser
               componentFactory:
                   (id<NewTabPageComponentFactoryProtocol>)componentFactory {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _componentFactory = componentFactory;
    _containerViewController = [[UIViewController alloc] init];
    _canfocusAccessibilityOmniboxWhenViewAppears = YES;
  }
  return self;
}

- (void)start {
  if (self.started) {
    return;
  }

  DCHECK(self.browser);
  DCHECK(self.toolbarDelegate);
  DCHECK(!self.contentSuggestionsCoordinator);

  self.webState = self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(self.webState);
  DCHECK(NewTabPageTabHelper::FromWebState(self.webState)->IsActive());

  // Start observing SceneState changes.
  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState addObserver:self];

  // Configures incognito NTP if user is in incognito mode.
  if (self.isOffTheRecord) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    self.started = YES;
    return;
  }

  // NOTE: anything that executes below WILL NOT execute for OffTheRecord
  // browsers!

  self.selectedFeed = NewTabPageTabHelper::FromWebState(self.webState)
                          ->GetNTPState()
                          .selectedFeed;

  [self initializeServices];
  [self initializeNTPComponents];
  [self startObservers];

  ProfileState* profileState = sceneState.profileState;
  [profileState addObserver:self];

  // Do not focus on omnibox for voice over if there are other screens to
  // show or if the caller requested for this focus to not happen.
  BOOL appInitializing = profileState.initStage < ProfileInitStage::kFinal;
  if (appInitializing || !self.canfocusAccessibilityOmniboxWhenViewAppears) {
    self.NTPViewController.focusAccessibilityOmniboxWhenViewAppears = NO;
  }

  // Update the feed if the account is subject to parental controls.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  signin::Tribool capability =
      supervised_user::IsPrimaryAccountSubjectToParentalControls(
          identityManager);
  [self updateFeedWithIsSupervisedUser:(capability == signin::Tribool::kTrue)];

  [self configureNTPMediator];
  if ([self.NTPMediator isFeedHeaderVisible]) {
    [self configureFeedAndHeader];
  }
  [self configureHeaderViewController];
  [self configureContentSuggestionsCoordinator];
  [self configureFeedMetricsRecorder];
  [self configureNTPViewController];
  if (IsTabGroupInGridEnabled()) {
    [self configureTabGroupIndicator];
  }

  self.started = YES;
}

- (void)stop {
  if (!self.started) {
    return;
  }

  _webState = nullptr;

  SceneState* sceneState = self.browser->GetSceneState();
  [sceneState removeObserver:self];

  if (self.isOffTheRecord) {
    self.incognitoViewController = nil;
    self.started = NO;
    return;
  }

  // NOTE: anything that executes below WILL NOT execute for OffTheRecord
  // browsers!

  [sceneState.profileState removeObserver:self];

  [_tabGroupIndicatorCoordinator stop];
  _tabGroupIndicatorCoordinator = nil;

  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.headerViewController = nil;
  // Remove before nil to ensure View Hierarchy doesn't hold last strong
  // reference.
  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];
  self.containedViewController = nil;
  [self.NTPViewController invalidate];
  self.NTPViewController = nil;
  self.feedHeaderViewController.NTPDelegate = nil;
  self.feedHeaderViewController = nil;
  [self.feedTopSectionCoordinator stop];
  self.feedTopSectionCoordinator = nil;
  [self stopAccountMenuCoordinator];
  [self stopSigninCoordinator];

  self.NTPMetricsRecorder = nil;

  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;

  self.authService = nil;
  self.templateURLService = nil;
  self.prefService = nil;

  [self.NTPMediator shutdown];
  self.NTPMediator = nil;

  if (self.feedViewController) {
    self.discoverFeedService->RemoveFeedViewController(self.feedViewController);
  }
  self.feedWrapperViewController = nil;
  self.feedViewController = nil;
  self.feedMetricsRecorder.followDelegate = nil;
  self.feedMetricsRecorder.NTPActionsDelegate = nil;
  self.feedMetricsRecorder = nil;

  _familyLinkUserCapabilitiesObserverBridge.reset();
  _discoverFeedObserverBridge.reset();
  _identityObserverBridge.reset();
  _authServiceObserverBridge.reset();
  [self clearPresentedState];

  [_customizationCoordinator stop];
  _customizationCoordinator = nil;

  [_fakeboxLensIconBubblePresenter dismissAnimated:NO];

  self.started = NO;
}

#pragma mark - Public

- (void)stopIfNeeded {
  WebStateList* webStateList = self.browser->GetWebStateList();
  for (int i = 0; i < webStateList->count(); i++) {
    NewTabPageTabHelper* iterNtpHelper =
        NewTabPageTabHelper::FromWebState(webStateList->GetWebStateAt(i));
    if (iterNtpHelper->IsActive()) {
      return;
    }
  }
  // No active NTPs were found.
  [self stop];
}

- (BOOL)isNTPActiveForCurrentWebState {
  if (!self.webState) {
    return NO;
  }
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  return NTPHelper && NTPHelper->IsActive();
}

- (BOOL)isScrolledToTop {
  return [self.NTPViewController isNTPScrolledToTop];
}

- (void)willUpdateSnapshot {
  if (self.contentSuggestionsCoordinator.started) {
    [self.NTPViewController willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  [self dismissCustomizationMenu];
  _fakeboxTapped = NO;
  [self.NTPViewController focusOmnibox];
}

- (void)reload {
  if (self.isOffTheRecord) {
    return;
  }
  [self.contentSuggestionsCoordinator refresh];
  // Call this before RefreshFeed() to ensure some NTP state configs are reset
  // before callbacks in repsonse to a feed refresh are called, ensuring the NTP
  // returns to a state at the top of the surface upon refresh.
  [self.NTPViewController resetStateUponReload];
  self.discoverFeedService->RefreshFeed(
      FeedRefreshTrigger::kForegroundUserTriggered);
}

- (void)locationBarDidBecomeFirstResponder {
  [self.NTPViewController omniboxDidBecomeFirstResponder];
}

- (void)locationBarWillResignFirstResponder {
  [self.NTPViewController omniboxWillResignFirstResponder];
}

- (void)locationBarDidResignFirstResponder {
  [self.NTPViewController omniboxDidResignFirstResponder];
}

- (void)constrainNamedGuideForFeedIPH {
  if (self.isOffTheRecord) {
    return;
  }
  [LayoutGuideCenterForBrowser(self.browser)
      referenceView:[self.headerViewController customizationMenuButton]
          underName:kFeedIPHNamedGuide];
}

- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent {
  // No-op.
}

- (void)handleFeedModelOfType:(FeedType)feedType
                didEndUpdates:(FeedLayoutUpdateType)updateType {
  DCHECK(self.NTPViewController);
  if (!self.feedViewController) {
    return;
  }
  // When the visible feed has been updated, recalculate the minimum NTP height.
  if (feedType == self.selectedFeed) {
    [self.NTPViewController feedLayoutDidEndUpdatesWithType:updateType];
  }
}

- (void)didNavigateToNTPInWebState:(web::WebState*)webState {
  CHECK(self.started);
  self.webState = webState;
  [self restoreNTPState];
  [self updateNTPIsVisible:YES];
  [self updateStartForVisibilityChange:YES];
  [self.toolbarDelegate didNavigateToNTPOnActiveWebState];
}

- (void)didNavigateAwayFromNTP {
  [self cancelOmniboxEdit];
  [self dismissCustomizationMenu];
  [self saveNTPState];
  [self updateNTPIsVisible:NO];
  [self updateStartForVisibilityChange:NO];
  self.webState = nullptr;
}

- (BOOL)isFakeboxPinned {
  if (self.isOffTheRecord) {
    return YES;
  }
  return self.NTPViewController.isFakeboxPinned;
}

- (void)presentLensIconBubble {
  if (!self.isScrolledToTop) {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:kMaterialDuration1
        animations:^{
          [weakSelf setContentOffsetToTop];
        }
        completion:^(BOOL finished) {
          [weakSelf presentLensIconBubbleNow];
        }];
    return;
  }

  [self presentLensIconBubbleNow];
}

- (BOOL)isFeedVisible {
  return [self.NTPMediator isFeedHeaderVisible] && self.feedViewController;
}

- (void)clearPresentedState {
  [self.contentSuggestionsCoordinator clearPresentedState];
  [self stopSharingCoordinator];
  [self stopAccountMenuCoordinator];
  [self stopSigninCoordinator];
}

#pragma mark - Setters

- (void)setSelectedFeed:(FeedType)selectedFeed {
  if (_selectedFeed == selectedFeed) {
    return;
  }
  // Updates the NTP state with the newly selected feed.
  [self saveNTPState];

  // Tell Metrics Recorder the feed has changed.
  [self.feedMetricsRecorder recordFeedTypeChangedFromFeed:_selectedFeed];
  _selectedFeed = selectedFeed;
}

#pragma mark - Initializers

// Gets all NTP services from the profile.
- (void)initializeServices {
  ProfileIOS* profile = self.profile;
  self.authService = AuthenticationServiceFactory::GetForProfile(profile);
  self.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  self.discoverFeedService = DiscoverFeedServiceFactory::GetForProfile(profile);
  self.prefService = profile->GetPrefs();
}

// Starts all NTP observers.
- (void)startObservers {
  DCHECK(self.prefService);
  DCHECK(self.headerViewController);

  // Start observing IdentityManager.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  _identityObserverBridge =
      std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                              self);

  // Start observing Family Link user capabilities.
  _familyLinkUserCapabilitiesObserverBridge = std::make_unique<
      supervised_user::FamilyLinkUserCapabilitiesObserverBridge>(
      identityManager, self);

  // Start observing DiscoverFeedService.
  _discoverFeedObserverBridge = std::make_unique<DiscoverFeedObserverBridge>(
      self, self.discoverFeedService);

  // Start observing Authentication service.
  _authServiceObserverBridge =
      std::make_unique<AuthenticationServiceObserverBridge>(self.authService,
                                                            self);
}

// Creates all the NTP components.
- (void)initializeNTPComponents {
  Browser* browser = self.browser;
  id<NewTabPageComponentFactoryProtocol> componentFactory =
      self.componentFactory;
  self.logoVendor = ios::provider::CreateLogoVendor(browser, self.webState);
  self.NTPViewController = [componentFactory NTPViewController];
  self.headerViewController = [componentFactory headerViewController];
  self.NTPMediator =
      [componentFactory NTPMediatorForBrowser:browser
                     identityDiscImageUpdater:self.headerViewController];
  self.contentSuggestionsCoordinator =
      [componentFactory contentSuggestionsCoordinatorForBrowser:browser];
  self.feedMetricsRecorder =
      [componentFactory feedMetricsRecorderForBrowser:browser];
  self.NTPMetricsRecorder = [[NewTabPageMetricsRecorder alloc] init];
}

#pragma mark - Configurators

// Creates and configures the feed and feed header based on user prefs.
- (void)configureFeedAndHeader {
  CHECK([self.NTPMediator isFeedHeaderVisible]);
  CHECK(self.NTPViewController);

  if (!self.feedHeaderViewController) {
    self.feedHeaderViewController =
        [self.componentFactory feedHeaderViewController];
  }

  self.feedHeaderViewController.feedControlDelegate = self;
  self.feedHeaderViewController.NTPDelegate = self;
  self.feedHeaderViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  if (!IsFollowUIUpdateEnabled()) {
    self.feedHeaderViewController.followingFeedSortType =
        self.followingFeedSortType;
  }
  self.NTPViewController.feedHeaderViewController =
      self.feedHeaderViewController;

  // Requests feeds here if the correct flags and prefs are enabled.
  if ([self.NTPMediator isFeedHeaderVisible]) {
    if ([self isFollowingFeedAvailable] &&
        self.selectedFeed == FeedTypeFollowing) {
      self.feedViewController = [self.componentFactory
              followingFeedForBrowser:self.browser
          viewControllerConfiguration:[self feedViewControllerConfiguration]
                             sortType:self.followingFeedSortType];
    } else {
      self.feedViewController = [self.componentFactory
               discoverFeedForBrowser:self.browser
          viewControllerConfiguration:[self feedViewControllerConfiguration]];
    }
  }

  // Feed top section visibility is based on feed visibility, so this should
  // always be below the block that sets `feedViewController`.
  if ([self isFeedVisible]) {
    self.feedTopSectionCoordinator = [self createFeedTopSectionCoordinator];
  }
}

// Configures `self.headerViewController`.
- (void)configureHeaderViewController {
  NewTabPageHeaderViewController* headerViewController =
      self.headerViewController;
  DCHECK(headerViewController);
  DCHECK(self.NTPMediator);
  DCHECK(self.NTPMetricsRecorder);

  headerViewController.isGoogleDefaultSearchEngine =
      [self isGoogleDefaultSearchEngine];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  headerViewController.fakeboxFocuserHandler =
      HandlerForProtocol(dispatcher, FakeboxFocuser);
  headerViewController.lensHandler =
      HandlerForProtocol(dispatcher, LensCommands);
  headerViewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  headerViewController.browserCoordinatorHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  headerViewController.helpHandler =
      HandlerForProtocol(dispatcher, HelpCommands);

  headerViewController.commandHandler = self;
  headerViewController.customizationDelegate = self;
  headerViewController.delegate = self.NTPViewController;
  headerViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  headerViewController.toolbarDelegate = self.toolbarDelegate;
  headerViewController.baseViewController = self.baseViewController;
  headerViewController.NTPMetricsRecorder = self.NTPMetricsRecorder;
  [headerViewController setLogoVendor:self.logoVendor];
}

// Configures `self.contentSuggestionsCoordinator`.
- (void)configureContentSuggestionsCoordinator {
  self.contentSuggestionsCoordinator.webState = self.webState;
  self.contentSuggestionsCoordinator.delegate = self;
  self.contentSuggestionsCoordinator.NTPActionsDelegate = self;
  self.contentSuggestionsCoordinator.homeStartDataSource = self;
  self.contentSuggestionsCoordinator.customizationDelegate = self;
  [self.contentSuggestionsCoordinator start];
}

// Configures `self.NTPMediator`.
- (void)configureNTPMediator {
  NewTabPageMediator* NTPMediator = self.NTPMediator;
  DCHECK(NTPMediator);
  NTPMediator.feedVisibilityObserver = self;
  NTPMediator.feedControlDelegate = self;
  NTPMediator.NTPContentDelegate = self;
  NTPMediator.headerConsumer = self.headerViewController;
  NTPMediator.consumer = self.NTPViewController;
  [NTPMediator setUp];
}

// Configures `self.feedMetricsRecorder`.
- (void)configureFeedMetricsRecorder {
  CHECK(self.webState);
  self.feedMetricsRecorder.NTPState =
      NewTabPageTabHelper::FromWebState(self.webState)->GetNTPState();
  self.feedMetricsRecorder.followDelegate = self;
  self.feedMetricsRecorder.NTPActionsDelegate = self;
}

// Configures `self.NTPViewController` and sets it up as the main ViewController
// managed by this Coordinator.
- (void)configureNTPViewController {
  DCHECK(self.NTPViewController);

  self.NTPViewController.magicStackCollectionView =
      self.contentSuggestionsCoordinator.magicStackCollectionView;
  self.NTPViewController.contentSuggestionsViewController =
      self.contentSuggestionsCoordinator.viewController;

  self.NTPViewController.feedVisible = [self isFeedVisible];

  self.feedWrapperViewController = [self.componentFactory
      feedWrapperViewControllerWithDelegate:self
                         feedViewController:self.feedViewController];
  self.NTPMediator.contentCollectionView =
      self.feedWrapperViewController.contentCollectionView;

  if ([self isFeedVisible]) {
    self.NTPViewController.feedTopSectionViewController =
        self.feedTopSectionCoordinator.viewController;
  }

  self.NTPViewController.feedWrapperViewController =
      self.feedWrapperViewController;
  self.NTPViewController.overscrollDelegate = self;
  self.NTPViewController.NTPContentDelegate = self;

  self.NTPViewController.headerViewController = self.headerViewController;

  [self configureMainViewControllerUsing:self.NTPViewController];
  self.NTPViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  self.NTPViewController.helpHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), HelpCommands);
  self.NTPViewController.mutator = self.NTPMediator;
}

// Configures the `_tabGroupIndicatorCoordinator` and sets the
// `tabGroupIndicatorView` to the `headerViewController`.
- (void)configureTabGroupIndicator {
  // The `_tabGroupIndicatorCoordinator` should be configured after the
  // `AdaptiveToolbarCoordinator` to gain access to the `NTPViewController`.
  _tabGroupIndicatorCoordinator = [[TabGroupIndicatorCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser];
  _tabGroupIndicatorCoordinator.toolbarHeightDelegate = nil;
  _tabGroupIndicatorCoordinator.displayedOnNTP = YES;
  [_tabGroupIndicatorCoordinator start];
  [self.headerViewController
      setTabGroupIndicatorView:_tabGroupIndicatorCoordinator.view];
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
  DCHECK(self.started);
  if (self.isOffTheRecord) {
    return self.incognitoViewController;
  } else {
    return self.containerViewController;
  }
}

#pragma mark - NewTabPageConfiguring

- (void)selectFeedType:(FeedType)feedType {
  if (!self.NTPViewController.viewDidAppear ||
      ![self isFollowingFeedAvailable]) {
    self.selectedFeed = feedType;
    return;
  }
  [self handleFeedSelected:feedType];
}

#pragma mark - NewTabPageHeaderCommands

- (void)updateForHeaderSizeChange {
  [self.NTPViewController updateHeightAboveFeed];
}

- (void)fakeboxTapped {
  [self dismissCustomizationMenu];
  _fakeboxTapped = YES;
  [self.NTPViewController focusOmnibox];
}

- (void)identityDiscWasTapped:(UIView*)identityDisc {
  if (_accountMenuCoordinator || _signinCoordinator) {
    // Double tap, or tap before dismissing of the previous one is complete.
    return;
  }
  [self dismissCustomizationMenu];
  [self.NTPMetricsRecorder recordIdentityDiscTapped];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  BOOL isSignedIn =
      self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (![self isSignInAllowed]) {
    [handler showSettingsFromViewController:self.baseViewController];
  } else if (isSignedIn) {
    if (IsIdentityDiscAccountMenuEnabled()) {
      [self showAccountMenu:identityDisc];
    } else {
      [handler showSettingsFromViewController:self.baseViewController];
    }
  } else {
    __weak __typeof(self) weakSelf = self;
    auto accessPoint = signin_metrics::AccessPoint::kNtpSignedOutIcon;
    auto promoAction =
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
    _signinCoordinator = [SigninCoordinator
        signinAndHistorySyncCoordinatorWithBaseViewController:
            self.baseViewController
                                                      browser:self.browser
                                                 contextStyle:
                                                     SigninContextStyle::
                                                         kDefault
                                                  accessPoint:accessPoint
                                                  promoAction:promoAction
                                          optionalHistorySync:YES
                                              fullscreenPromo:NO
                                         continuationProvider:
                                             DoNothingContinuationProvider()];
    _signinCoordinator.signinCompletion = ^(
        SigninCoordinatorResult result, id<SystemIdentity> completionIdentity) {
      [weakSelf showSigninCommandDidFinish];
    };
    [_signinCoordinator start];
  }
}

- (void)customizationMenuWasTapped:(UIView*)customizationMenu {
  if (_customizationCoordinator) {
    // The menu is already opened, so tapping an entrypoint again should close
    // it.
    [self dismissCustomizationMenu];
    return;
  }

  PrefService* localState = GetApplicationContext()->GetLocalState();
  if (localState->GetInteger(
          prefs::kNTPHomeCustomizationNewBadgeImpressionCount) <=
      kCustomizationNewBadgeMaxImpressionCount) {
    base::RecordAction(
        base::UserMetricsAction(kNTPCustomizationNewBadgeTappedAction));
    // Set the new badge impression count to `INT_MAX` to ensure it isn't shown
    // again, even if we increase the max impression count.
    localState->SetInteger(prefs::kNTPHomeCustomizationNewBadgeImpressionCount,
                           INT_MAX);

    [self.headerViewController hideBadgeOnCustomizationMenu];
  }

  [self.NTPMetricsRecorder recordHomeCustomizationMenuOpenedFromEntrypoint:
                               HomeCustomizationEntrypoint::kMain];

  [self openCustomizationMenuAtPage:CustomizationMenuPage::kMain animated:YES];
}

#pragma mark - DiscoverFeedVisibilityObserver

- (void)didChangeDiscoverFeedVisibility {
  // TODO(crbug.com/412691611): Consider moving to mediator after refactor.
  if (!self.NTPViewController.viewLoaded) {
    return;
  }
  [self handleChangeInModules];
  [self.NTPViewController setContentOffsetToTop];
  [self updateModuleVisibility];
}

#pragma mark - DiscoverFeedPreviewDelegate

- (UIViewController*)discoverFeedPreviewWithURL:(const GURL)URL {
  std::string referrerURL = base::GetFieldTrialParamValueByFeature(
      kOverrideFeedSettings, kFeedSettingDiscoverReferrerParameter);
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

#pragma mark - FeedControlDelegate

- (FollowingFeedSortType)followingFeedSortType {
  // TODO(crbug.com/40858105): Add a DCHECK to make sure the coordinator isn't
  // stopped when we check this. That would require us to use the NTPHelper to
  // get this information.
  return (FollowingFeedSortType)self.prefService->GetInteger(
      prefs::kNTPFollowingFeedSortType);
}

- (void)handleFeedSelected:(FeedType)feedType {
  DCHECK([self isFollowingFeedAvailable]);

  if (self.selectedFeed == feedType) {
    return;
  }
  self.selectedFeed = feedType;

  // Saves scroll position before changing feed.
  CGFloat scrollPosition = [self.NTPViewController scrollPosition];

  [self handleChangeInModules];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.NTPViewController setContentOffsetToTopOfFeedOrLess:scrollPosition];
}

- (void)handleSortTypeForFollowingFeed:(FollowingFeedSortType)sortType {
  DCHECK([self isFollowingFeedAvailable]);

  if (self.feedHeaderViewController.followingFeedSortType == sortType) {
    return;
  }

  // Save the scroll position before changing sort type.
  CGFloat scrollPosition = [self.NTPViewController scrollPosition];

  [self.feedMetricsRecorder recordFollowingFeedSortTypeSelected:sortType];
  self.prefService->SetInteger(prefs::kNTPFollowingFeedSortType, sortType);
  self.prefService->SetBoolean(prefs::kDefaultFollowingFeedSortTypeChanged,
                               true);
  self.discoverFeedService->SetFollowingFeedSortType(sortType);
  self.feedHeaderViewController.followingFeedSortType = sortType;

  [self handleChangeInModules];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.NTPViewController setContentOffsetToTopOfFeedOrLess:scrollPosition];

  // Updates the NTP state for the newly selected sort type.
  [self saveNTPState];
}

- (BOOL)isFollowingFeedAvailable {
  return IsWebChannelsEnabled() && self.authService &&
         self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

- (NSUInteger)lastVisibleFeedCardIndex {
  return [self.feedWrapperViewController lastVisibleFeedCardIndex];
}

- (void)updateFeedForDefaultSearchEngineChanged {
  if (!self.NTPViewController.viewLoaded) {
    return;
  }
  [self.feedHeaderViewController updateForDefaultSearchEngineChanged];
  [self updateFeedLayout];
  [self cancelOmniboxEdit];
  [self.NTPViewController setContentOffsetToTop];
}

#pragma mark - ContentSuggestionsDelegate

- (void)contentSuggestionsWasUpdated {
  [self.NTPViewController updateHeightAboveFeed];
}

- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::MostVisitedEntry];
  _sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [_sharingCoordinator start];
}

- (void)openMagicStackCustomizationMenu {
  if (_customizationCoordinator) {
    // The menu is already opened, so tapping an entrypoint again should close
    // it.
    [self dismissCustomizationMenu];
    return;
  }

  [self.NTPMetricsRecorder recordHomeCustomizationMenuOpenedFromEntrypoint:
                               HomeCustomizationEntrypoint::kMagicStack];

  [self openCustomizationMenuAtPage:CustomizationMenuPage::kMagicStack
                           animated:NO];
}

#pragma mark - FeedSignInPromoDelegate

- (void)showSignInUIFromSource:(FeedSignInPromoSource)source {
  // If the user is already signed in, do nothing.
  if (self.authService &&
      self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }
  // This flow shouldn't be offered if sign-in is disallowed.
  // In theory, the flow should not even have been offered to the user.
  if (![self isSignInAllowed]) {
    [self showSignInDisableMessage];
    [self.feedMetricsRecorder recordShowSyncnRelatedUIWithType:
                                  feed::FeedSyncPromo::kShowDisableToast];
    return;
  }
  if (_accountMenuCoordinator || _signinCoordinator) {
    return;
  }
  BOOL hasUserIdentities = [self hasIdentitiesOnDevice];

  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kNtpFeedCardMenuPromo;
  switch (source) {
    case FeedSignInCommandSourceBottom:
      // TODO(crbug.com/40066051): Strictly speaking this should record a bucket
      // other than kShowSyncFlow. But I don't think we care too much about this
      // particular histogram, just rename the bucket after launch.
      [self.feedMetricsRecorder
          recordShowSyncnRelatedUIWithType:feed::FeedSyncPromo::kShowSyncFlow];
      break;
    case FeedSignInCommandSourceCardMenu:
      accessPoint = signin_metrics::AccessPoint::kNtpFeedBottomPromo;
      [self.feedMetricsRecorder recordShowSignInRelatedUIWithType:
                                    feed::FeedSignInUI::kShowSignInOnlyFlow];
      [self.feedMetricsRecorder
          recordShowSignInOnlyUIWithUserId:hasUserIdentities];
      break;
  }
  __weak __typeof(self) weakSelf = self;
  // If there are 0 identities, kInstantSignin requires less taps.
  if (hasUserIdentities) {
    _signinCoordinator = [SigninCoordinator
        consistencyPromoSigninCoordinatorWithBaseViewController:
            self.NTPViewController
                                                        browser:self.browser
                                                   contextStyle:
                                                       SigninContextStyle::
                                                           kDefault
                                                    accessPoint:accessPoint
                                           prepareChangeProfile:nil
                                           continuationProvider:
                                               DoNothingContinuationProvider()];
  } else {
    _signinCoordinator = [SigninCoordinator
        instantSigninCoordinatorWithBaseViewController:self.NTPViewController
                                               browser:self.browser
                                              identity:nil
                                          contextStyle:SigninContextStyle::
                                                           kDefault
                                           accessPoint:accessPoint
                                           promoAction:
                                               signin_metrics::PromoAction::
                                                   PROMO_ACTION_NO_SIGNIN_PROMO
                                  continuationProvider:
                                      DoNothingContinuationProvider()];
  }
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, id<SystemIdentity> completionIdentity) {
        [weakSelf showSigninCommandDidFinish];
      };
  [_signinCoordinator start];
  signin_metrics::RecordSigninUserActionForAccessPoint(accessPoint);
}

#pragma mark - FeedWrapperViewControllerDelegate

- (void)updateTheme {
  self.discoverFeedService->UpdateTheme();
}

#pragma mark - NewTabPageContentDelegate

- (void)signinPromoHasChangedVisibility:(BOOL)visible {
  [self.feedTopSectionCoordinator signinPromoHasChangedVisibility:visible];
}

- (void)cancelOmniboxEdit {
  id<OmniboxCommands> omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandHandler cancelOmniboxEdit];
}

- (void)onFakeboxBlur {
  id<FakeboxFocuser> fakeboxFocuserHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), FakeboxFocuser);
  [fakeboxFocuserHandler onFakeboxBlur];
}

- (void)focusOmnibox {
  id<FakeboxFocuser> fakeboxFocuserHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), FakeboxFocuser);
  [fakeboxFocuserHandler focusOmniboxFromFakebox:_fakeboxTapped
                                          pinned:[self isFakeboxPinned]
                  fakeboxButtonsSnapshotProvider:self.headerViewController];
}

- (void)refreshNTPContent {
  self.discoverFeedService->RefreshFeed(
      FeedRefreshTrigger::kForegroundFeedVisibleOther);
}

- (void)updateForSelectedFeed:(FeedType)selectedFeed {
  [self selectFeedType:selectedFeed];
  if (!IsFollowUIUpdateEnabled()) {
    // Reassign the sort type in case it changed in another tab.
    self.feedHeaderViewController.followingFeedSortType =
        self.followingFeedSortType;
  }
  // Update the header so that it's synced with the currently selected
  // feed, which could have been changed when a new web state was
  // inserted.
  [self.feedHeaderViewController updateForSelectedFeed];
  self.feedMetricsRecorder.followDelegate = self;
}

- (void)updateModuleVisibility {
  [_customizationCoordinator updateMenuData];
  [self handleChangeInModules];
  [self cancelOmniboxEdit];
  [self setContentOffsetToTop];
  [self.feedHeaderViewController updateForFeedVisibilityChanged];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  nil);
}

- (void)feedDidScroll {
  feature_engagement::TrackerFactory::GetForProfile(self.profile)
      ->NotifyEvent(feature_engagement::events::kIOSScrolledOnFeed);
}

#pragma mark - NewTabPageDelegate

- (void)updateFeedLayout {
  // If this coordinator has not finished [self start], the below will start
  // viewDidLoad before the UI is ready, failing DCHECKS.
  if (!self.started) {
    return;
  }
  // TODO(crbug.com/40252945): Investigate why this order is correct. Intuition
  // would be that the layout update should happen before telling UIKit to
  // relayout.
  [self.containedViewController.view setNeedsLayout];
  [self.containedViewController.view layoutIfNeeded];
  [self.NTPViewController updateNTPLayout];
}

- (void)setContentOffsetToTop {
  [self.NTPViewController setContentOffsetToTop];
}

- (BOOL)isGoogleDefaultSearchEngine {
  return search::DefaultSearchProviderIsGoogle(self.templateURLService);
}

- (BOOL)isStartSurface {
  // The web state is nil if the NTP is in another tab. In this case, it is
  // never a start surface.
  if (!self.webState) {
    return NO;
  }
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  return NTPHelper && NTPHelper->ShouldShowStartSurface();
}

- (void)handleFeedTopSectionClosed {
  [self.NTPViewController updateScrollPositionForFeedTopSectionClosed];
}

- (BOOL)isSignInAllowed {
  AuthenticationService::ServiceStatus statusService =
      self.authService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser: {
      return NO;
    }
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed: {
      break;
    }
  }
  return YES;
}

#pragma mark - NewTabPageFollowDelegate

- (NSUInteger)followedPublisherCount {
  return self.followedWebSites.count;
}

- (BOOL)doesFollowingFeedHaveContent {
  for (FollowedWebSite* web_site in self.followedWebSites) {
    if (web_site.state == FollowedWebSiteStateStateActive) {
      return YES;
    }
  }

  return NO;
}

- (NSArray<FollowedWebSite*>*)followedWebSites {
  FollowBrowserAgent* followBrowserAgent =
      FollowBrowserAgent::FromBrowser(self.browser);

  // Return an empty list if the BrowserAgent is null (which can happen
  // if e.g. the Browser is off-the-record).
  if (!followBrowserAgent) {
    return @[];
  }

  return followBrowserAgent->GetFollowedWebSites();
}

#pragma mark - NewTabPageActionsDelegate

- (void)recentTabTileOpenedAtIndex:(NSUInteger)index {
  RecordMagicStackClick(ContentSuggestionsModuleType::kTabResumption,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kReturnToRecentTab,
                   [self isStartSurface]);
  RecordMagicStackTabResumptionClick(true, [self isStartSurface], index);
}

- (void)distantTabResumptionOpenedAtIndex:(NSUInteger)index {
  RecordMagicStackClick(ContentSuggestionsModuleType::kTabResumption,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kOpenDistantTabResumption,
                   [self isStartSurface]);
  RecordMagicStackTabResumptionClick(false, [self isStartSurface], index);
}

- (void)recentTabTileDisplayedAtIndex:(NSUInteger)index {
  LogTabResumptionImpression(true, [self isStartSurface], index);
}

- (void)distantTabResumptionDisplayedAtIndex:(NSUInteger)index {
  LogTabResumptionImpression(false, [self isStartSurface], index);
}

- (void)feedArticleOpened {
  RecordHomeAction(IOSHomeActionType::kFeedCard, [self isStartSurface]);
}

- (void)mostVisitedTileOpened {
  RecordHomeAction(IOSHomeActionType::kMostVisitedTile, [self isStartSurface]);
}

- (void)shortcutTileOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kShortcuts,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kShortcuts, [self isStartSurface]);
  [self dismissCustomizationMenu];
}

- (void)setUpListItemOpened {
  RecordHomeAction(IOSHomeActionType::kSetUpList, [self isStartSurface]);
  [self dismissCustomizationMenu];
}

- (void)safetyCheckOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kSafetyCheck,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kSafetyCheck, [self isStartSurface]);
  [self dismissCustomizationMenu];
}

- (void)priceTrackingPromoOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kPriceTrackingPromo,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kPriceTrackingPromo,
                   [self isStartSurface]);
}

- (void)tipsOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kTips,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kTips, [self isStartSurface]);
  [self dismissCustomizationMenu];
}

- (void)shopCardOpened {
  RecordMagicStackClick(ContentSuggestionsModuleType::kShopCard,
                        [self isStartSurface]);
  RecordHomeAction(IOSHomeActionType::kShopCard, [self isStartSurface]);
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommandsHandler openURLInNewTab:[OpenNewTabCommand command]];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kOpenedNewTab];
}

- (void)overscrollActionCloseTab:(OverscrollActionsController*)controller {
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler closeCurrentTab];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kCloseTab];
}

- (void)overscrollActionRefresh:(OverscrollActionsController*)controller {
  [self reload];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kPullToRefresh];
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return !_customizationCoordinator;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return nil;
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.feedWrapperViewController.view;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [self.NTPViewController heightAboveFeed];
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat height = [self.headerViewController toolBarView].bounds.size.height;
  CGFloat topInset = self.feedWrapperViewController.view.safeAreaInsets.top;
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

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage == ProfileInitStage::kFinal) {
    self.NTPViewController.focusAccessibilityOmniboxWhenViewAppears = YES;
    [self.headerViewController focusAccessibilityOnOmnibox];

    [profileState removeObserver:self];
  }
}

#pragma mark - DiscoverFeedObserverBridge

- (void)discoverFeedModelWasCreated {
  if (self.NTPViewController.viewDidAppear) {
    [self handleChangeInModules];

    if (IsWebChannelsEnabled()) {
      [self.feedHeaderViewController updateForFollowingFeedVisibilityChanged];
      [self updateFeedLayout];
    }
    [self.NTPViewController setContentOffsetToTop];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// TODO(crbug.com/346756363): Remove this method as it is replaced with
// `onIsSubjectToParentalControlsCapabilityChanged`.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // An account change may trigger after the coordinator has been stopped.
  // In this case do not process the event.
  if (!self.started) {
    return;
  }
  signin::PrimaryAccountChangeEvent::Type eventType =
      event.GetEventTypeFor(signin::ConsentLevel::kSignin);
  switch (eventType) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      [self.contentSuggestionsCoordinator refresh];
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - AuthenticationServiceObserving

- (void)onServiceStatusChanged {
  switch (self.authService->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      // If sign-in becomes disabled, the sign-in promo must be disabled too.
      // TODO(crbug.com/40280872): The sign-in promo should just be hidden
      // instead of resetting the hierarchy.
      [self handleChangeInModules];
      [self setContentOffsetToTop];
  }
}

#pragma mark - FamilyLinkUserCapabilitiesObserving

- (void)onIsSubjectToParentalControlsCapabilityChanged:
    (supervised_user::CapabilityUpdateState)capabilityUpdateState {
  BOOL isSubjectToParentalControl =
      (capabilityUpdateState ==
       supervised_user::CapabilityUpdateState::kSetToTrue);
  [self updateFeedWithIsSupervisedUser:isSubjectToParentalControl];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // `SceneActivationLevelForegroundInactive` is called both when foregrounding
  // and backgrounding, and is thus not used as an indicator to trigger
  // visibility.
  if (self.webState && !self.visible &&
      level == SceneActivationLevelForegroundActive) {
    [self updateNTPIsVisible:YES];
  } else if (self.visible && level < SceneActivationLevelForegroundInactive) {
    [self updateNTPIsVisible:NO];
  }
}

#pragma mark - AccountMenuCoordinatorDelegate

// Update the state, to take into account that the account menu coordinator is
// stopped.
- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  CHECK_EQ(_accountMenuCoordinator, coordinator, base::NotFatalUntil::M140);
  [self stopAccountMenuCoordinator];
}

#pragma mark - Private

- (void)stopSharingCoordinator {
  [_sharingCoordinator stop];
  _sharingCoordinator = nil;
}

- (void)stopAccountMenuCoordinator {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

- (void)showAccountMenu:(UIView*)identityDisc {
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser
                      anchorView:identityDisc
                     accessPoint:AccountMenuAccessPoint::kNewTabPage
                             URL:GURL()];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

- (bool)hasIdentitiesOnDevice {
  return !IdentityManagerFactory::GetForProfile(self.profile)
              ->GetAccountsOnDevice()
              .empty();
}

// Update the state, to take into account that the account menu coordinator is
// stopped.
- (void)showAccountMenuDidFinish {
  [self stopAccountMenuCoordinator];
}

// Update the state, to take into account that the signin coordinator
// coordinator is stopped.
- (void)showSigninCommandDidFinish {
  CHECK(_signinCoordinator, base::NotFatalUntil::M140);
  [self stopSigninCoordinator];
}

// Updates the feed visibility or content based on the supervision state
// of the account defined in `value`.
- (void)updateFeedWithIsSupervisedUser:(BOOL)value {
  // This may be called asynchronously after the NTP has
  // been stopped and the object has been stopped. Ignore
  // the invocation.
  PrefService* prefService = self.prefService;
  if (!prefService) {
    return;
  }

  prefService->SetBoolean(prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
                          !value);
}

- (void)updateStartForVisibilityChange:(BOOL)visible {
  if (visible && NewTabPageTabHelper::FromWebState(self.webState)
                     ->ShouldShowStartSurface()) {
    DiscoverFeedServiceFactory::GetForProfile(self.profile)
        ->SetIsShownOnStartSurface(true);
  }
  if (!visible && NewTabPageTabHelper::FromWebState(self.webState)
                      ->ShouldShowStartSurface()) {
    // This means the NTP going away was showing Start. Reset configuration
    // since it should not show Start after disappearing.
    NewTabPageTabHelper::FromWebState(self.webState)
        ->SetShowStartSurface(false);
  }
}

// Updates the NTP to take into account a change in module visibility
- (void)handleChangeInModules {
  DCHECK(self.NTPViewController);

  [self.NTPViewController resetViewHierarchy];

  if (self.feedViewController) {
    self.discoverFeedService->RemoveFeedViewController(self.feedViewController);
  }

  [self.feedTopSectionCoordinator stop];

  self.NTPViewController.feedWrapperViewController = nil;
  self.NTPViewController.feedTopSectionViewController = nil;
  self.feedWrapperViewController = nil;
  self.feedViewController = nil;
  self.feedTopSectionCoordinator = nil;

  // Fetches feed header and conditionally fetches feed. Feed can only be
  // visible if feed header is visible.
  if ([self.NTPMediator isFeedHeaderVisible]) {
    [self configureFeedAndHeader];
  } else {
    self.NTPViewController.feedHeaderViewController = nil;
    self.feedHeaderViewController = nil;
  }

  if ([self isFeedVisible]) {
    self.NTPViewController.feedTopSectionViewController =
        self.feedTopSectionCoordinator.viewController;
  }

  self.NTPViewController.feedVisible = [self isFeedVisible];

  self.feedWrapperViewController = [self.componentFactory
      feedWrapperViewControllerWithDelegate:self
                         feedViewController:self.feedViewController];

  self.NTPViewController.feedWrapperViewController =
      self.feedWrapperViewController;
  self.NTPMediator.contentCollectionView =
      self.feedWrapperViewController.contentCollectionView;

  [self.NTPViewController layoutContentInParentCollectionView];

  [self updateFeedLayout];
}

// Creates, configures and returns a feed view controller configuration.
- (DiscoverFeedViewControllerConfiguration*)feedViewControllerConfiguration {
  DiscoverFeedViewControllerConfiguration* viewControllerConfig =
      [[DiscoverFeedViewControllerConfiguration alloc] init];
  viewControllerConfig.browser = self.browser;
  viewControllerConfig.scrollDelegate = self.NTPViewController;
  viewControllerConfig.previewDelegate = self;
  viewControllerConfig.signInPromoDelegate = self;

  return viewControllerConfig;
}

// Configures and returns the feed top section coordinator.
- (FeedTopSectionCoordinator*)createFeedTopSectionCoordinator {
  DCHECK(self.NTPViewController);
  FeedTopSectionCoordinator* feedTopSectionCoordinator =
      [[FeedTopSectionCoordinator alloc]
          initWithBaseViewController:self.NTPViewController
                             browser:self.browser];
  feedTopSectionCoordinator.NTPDelegate = self;
  [feedTopSectionCoordinator start];
  return feedTopSectionCoordinator;
}

// Private setter for the `webState` property.
- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }

  _webState = webState;
  self.contentSuggestionsCoordinator.webState = _webState;
  [self.logoVendor setWebState:_webState];
}

// Called when the NTP changes visibility, either when the user navigates to
// or away from the NTP, or when the active WebState changes.
- (void)updateNTPIsVisible:(BOOL)visible {
  if (visible == self.visible) {
    return;
  }

  CHECK(self.webState);

  self.visible = visible;
  self.NTPViewController.NTPVisible = visible;
  self.NTPMediator.NTPVisible = visible;

  if (!self.isOffTheRecord) {
    if (visible) {
      self.didAppearTime = base::TimeTicks::Now();

      [self.NTPMetricsRecorder
          recordCustomizationState:[self currentCustomizationState]];

      PrefService* prefService = self.prefService;
      BOOL safetyCheckEnabled = prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackSafetyCheckEnabled);
      BOOL setUpListEnabled = prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackSetUpListEnabled);
      BOOL tabResumptionEnabled = prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTabResumptionEnabled);
      BOOL parcelTrackingEnabled = prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackParcelTrackingEnabled);
      BOOL tipsEnabled = prefService->GetBoolean(
          prefs::kHomeCustomizationMagicStackTipsEnabled);
      [self.NTPMetricsRecorder
          recordMagicStackCustomizationStateWithSetUpList:setUpListEnabled
                                              safetyCheck:safetyCheckEnabled
                                            tabResumption:tabResumptionEnabled
                                           parcelTracking:parcelTrackingEnabled
                                                     tips:tipsEnabled];

      // TODO(crbug.com/350990359): Deprecate IOS.NTP.Impression when Home
      // Customization launches.
      if ([self.NTPMediator isFeedHeaderVisible]) {
        [self.NTPMetricsRecorder
            recordHomeImpression:IOSNTPImpressionType::kFeedVisible
                  isStartSurface:[self isStartSurface]];
      } else {
        [self.NTPMetricsRecorder
            recordHomeImpression:IOSNTPImpressionType::kFeedDisabled
                  isStartSurface:[self isStartSurface]];
      }
    } else {
      if (!self.didAppearTime.is_null()) {
        [self.NTPMetricsRecorder
            recordTimeSpentInHome:(base::TimeTicks::Now() - self.didAppearTime)
                   isStartSurface:[self isStartSurface]];
        self.didAppearTime = base::TimeTicks();
      }
    }
    // Check if feed is visible before reporting NTP visibility as the feed
    // needs to be visible in order to use for metrics.
    // TODO(crbug.com/40871863) Move isFeedVisible check to the metrics
    // recorder
    if ([self isFeedVisible]) {
      [self.feedMetricsRecorder recordNTPDidChangeVisibility:visible];
    }
  }
}

// Returns whether the user policies allow them to sync.
- (BOOL)isSyncAllowedByPolicy {
  return !SyncServiceFactory::GetForProfile(self.profile)
              ->HasDisableReason(
                  syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

// Shows sign-in disabled snackbar message.
- (void)showSignInDisableMessage {
  id<SnackbarCommands> handler =
      static_cast<id<SnackbarCommands>>(self.browser->GetCommandDispatcher());
  MDCSnackbarMessage* message = CreateSnackbarMessage(l10n_util::GetNSString(
      IDS_IOS_NTP_FEED_SIGNIN_PROMO_DISABLE_SNACKBAR_MESSAGE));

  [handler showSnackbarMessage:message];
}

// Saves the state of the NTP associated with `self.webState`.
- (void)saveNTPState {
  [self.NTPMediator saveNTPStateForWebState:self.webState];
}

// Restores the saved state of the NTP associated with `self.webState` if
// necessary.
- (void)restoreNTPState {
  [self.NTPMediator restoreNTPStateForWebState:self.webState];
}

// Opens the Home customization menu at a specific `page`.
- (void)openCustomizationMenuAtPage:(CustomizationMenuPage)page
                           animated:(BOOL)animated {
  _customizationCoordinator = [[HomeCustomizationCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser];
  _customizationCoordinator.delegate = self;
  [_customizationCoordinator start];
  [_customizationCoordinator presentCustomizationMenuPage:page];
  feature_engagement::TrackerFactory::GetForProfile(self.profile)
      ->NotifyEvent(feature_engagement::events::kHomeCustomizationMenuUsed);
}

// Returns the current customization state represnting the visibility of NTP
// components.
- (IOSNTPImpressionCustomizationState)currentCustomizationState {
  PrefService* prefService = self.prefService;
  BOOL MVTEnabled =
      prefService->GetBoolean(prefs::kHomeCustomizationMostVisitedEnabled);
  BOOL magicStackEnabled =
      prefService->GetBoolean(prefs::kHomeCustomizationMagicStackEnabled);
  BOOL feedEnabled = [self.NTPMediator isFeedHeaderVisible];

  // All components enabled/disabled.
  if (MVTEnabled && magicStackEnabled && feedEnabled) {
    return IOSNTPImpressionCustomizationState::kAllEnabled;
  }
  if (!MVTEnabled && !magicStackEnabled && !feedEnabled) {
    return IOSNTPImpressionCustomizationState::kAllDisabled;
  }

  // 2 components enabled.
  if (MVTEnabled && magicStackEnabled && !feedEnabled) {
    return IOSNTPImpressionCustomizationState::kMVTAndMagicStackEnabled;
  }
  if (MVTEnabled && !magicStackEnabled && feedEnabled) {
    return IOSNTPImpressionCustomizationState::kMVTAndFeedEnabled;
  }
  if (!MVTEnabled && magicStackEnabled && feedEnabled) {
    return IOSNTPImpressionCustomizationState::kMagicStackAndFeedEnabled;
  }

  // 1 component enabled.
  if (MVTEnabled && !magicStackEnabled && !feedEnabled) {
    return IOSNTPImpressionCustomizationState::kMVTEnabled;
  }
  if (!MVTEnabled && magicStackEnabled && !feedEnabled) {
    return IOSNTPImpressionCustomizationState::kMagicStackEnabled;
  }
  if (!MVTEnabled && !magicStackEnabled && feedEnabled) {
    return IOSNTPImpressionCustomizationState::kFeedEnabled;
  }

  NOTREACHED();
}

// Presents the Fakebox Lens icon IPH bubble without checking scroll position.
- (void)presentLensIconBubbleNow {
  NSString* text = l10n_util::GetNSString(IDS_IOS_LENS_PROMO_IPH_TEXT);
  UIView* icon = [LayoutGuideCenterForBrowser(self.browser)
      referencedViewUnderName:kFakeboxLensIconGuide];
  CGPoint anchorPoint = [icon.superview convertPoint:icon.frame.origin
                                              toView:nil];
  anchorPoint.x += icon.frame.size.width / 2;
  anchorPoint.y += icon.frame.size.height;

  BubbleViewControllerPresenter* presenter =
      [[BubbleViewControllerPresenter alloc]
          initDefaultBubbleWithText:text
                     arrowDirection:BubbleArrowDirectionUp
                          alignment:BubbleAlignmentBottomOrTrailing
                  dismissalCallback:nil];
  // Discard if it doesn't fit in the view as it is currently shown.
  if (![presenter canPresentInView:self.NTPViewController.view
                       anchorPoint:anchorPoint]) {
    return;
  }
  [presenter presentInViewController:self.NTPViewController
                         anchorPoint:anchorPoint];
  _fakeboxLensIconBubblePresenter = presenter;
}

#pragma mark - HomeCustomizationDelegate

- (void)dismissCustomizationMenu {
  // Return early if the customization menu is not presented to avoid dismissing
  // another view controller.
  if (!_customizationCoordinator) {
    return;
  }
  [_customizationCoordinator stop];
  _customizationCoordinator = nil;
}

@end
