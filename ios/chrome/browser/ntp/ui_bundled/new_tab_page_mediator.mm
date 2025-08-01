// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/omnibox/browser/omnibox_prefs.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/search/search.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/aim/model/aim_availability.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_notifier_browser_agent.h"
#import "ios/chrome/browser/browser_view/model/browser_view_visibility_observer_bridge.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_mediator.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/user_account_image_update_delegate.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_visibility_browser_agent.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer_bridge.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_state.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_control_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_consumer.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ntp/ui_bundled/theme_utils.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"
#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service_observer_bridge.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Histogram name for logging when the 'new' badge on the Lens button is shown
// on the homepage.
constexpr char kNTPLensButtonNewBadgeShownHistogram[] =
    "IOS.NTP.LensButtonNewBadgeShown";

// These values are persisted to `IOS.NTP.LensButtonNewBadgeShown` histograms.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSNTPNewBadgeShownResult {
  kShown = 0,
  // kNotShownLimitReached = 1,  // Obsolete in M140
  // kNotShownButtonPressed = 2,  // Obsolete in M140
  kMaxValue = kShown,
};

// The point size of the entry point's symbol.
const CGFloat kIconPointSize = 18.0;

// The holdback period to wait after FRE completion before showing new badges
// on the homepage.
constexpr base::TimeDelta kFREBadgeHoldbackPeriod = base::Hours(1);

// Logs when the 'new' badge on the homepage Lens button is shown.
//
// TODO(crbug.com/428691449): Remove once the FET migration for 'new' badges is
// fully validated.
void LogLensButtonNewBadgeShownHistogram(IOSNTPNewBadgeShownResult result) {
  base::UmaHistogramEnumeration(kNTPLensButtonNewBadgeShownHistogram, result);
}

}  // namespace

@interface NewTabPageMediator () <BrowserViewVisibilityObserving,
                                  HomeBackgroundCustomizationServiceObserving,
                                  IdentityManagerObserverBridgeDelegate,
                                  PlaceholderServiceObserving,
                                  PrefObserverDelegate,
                                  SearchEngineObserving,
                                  SyncObserverModelBridge>

@property(nonatomic, assign) ChromeAccountManagerService* accountManagerService;
// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;
// Authentication Service to get the current user's avatar.
@property(nonatomic, assign) AuthenticationService* authService;
// This is the object that knows how to update the Identity Disc UI.
@property(nonatomic, weak) id<UserAccountImageUpdateDelegate> imageUpdater;
// DiscoverFeed Service to display the Feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;

@end

@implementation NewTabPageMediator {
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Observes changes in identity and updates the Identity Disc.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Observes changes of the browser view visibility state.
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent>
      _browserViewVisibilityNotifierBrowserAgent;
  // Observes changes of the feed visibility state.
  raw_ptr<DiscoverFeedVisibilityBrowserAgent>
      _discoverFeedVisibilityBrowserAgent;
  std::unique_ptr<BrowserViewVisibilityObserverBridge>
      _browserViewVisibilityObserverBridge;
  // Used to load URLs.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  raw_ptr<PrefService> _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // The current default search engine.
  raw_ptr<const TemplateURL> _defaultSearchEngine;
  // Sync Service.
  raw_ptr<syncer::SyncService> _syncService;
  // Used to check feed configuration based on the country.
  raw_ptr<regional_capabilities::RegionalCapabilitiesService>
      _regionalCapabilitiesService;
  // Used to get and observe the background image or other state.
  raw_ptr<HomeBackgroundCustomizationService> _backgroundCustomizationService;
  // Observer for the customization service.
  std::unique_ptr<HomeBackgroundCustomizationServiceObserverBridge>
      _backgroundCustomizationServiceObserverBridge;
  // Used to fetch and cache images for the background.
  raw_ptr<image_fetcher::ImageFetcherService> _imageFetcherService;
  // Observer to keep track of the syncing status.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  raw_ptr<signin::IdentityManager> _identityManager;
  id<SystemIdentity> _signedInIdentity;
  std::unique_ptr<PlaceholderServiceObserverBridge> _placeholderServiceObserver;
  // Feature engagement tracker for handling "new" badge IPH.
  raw_ptr<feature_engagement::Tracker> _tracker;
}

// Synthesized from NewTabPageMutator.
@synthesize scrollPositionToSave = _scrollPositionToSave;

- (instancetype)
            initWithTemplateURLService:(TemplateURLService*)templateURLService
                             URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                           authService:(AuthenticationService*)authService
                       identityManager:(signin::IdentityManager*)identityManager
                 accountManagerService:
                     (ChromeAccountManagerService*)accountManagerService
              identityDiscImageUpdater:
                  (id<UserAccountImageUpdateDelegate>)imageUpdater
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
                           prefService:(PrefService*)prefService
                           syncService:(syncer::SyncService*)syncService
           regionalCapabilitiesService:
               (regional_capabilities::RegionalCapabilitiesService*)
                   regionalCapabilitiesService
        backgroundCustomizationService:
            (HomeBackgroundCustomizationService*)backgroundCustomizationService
                   imageFetcherService:
                       (image_fetcher::ImageFetcherService*)imageFetcherService
         browserViewVisibilityNotifier:
             (BrowserViewVisibilityNotifierBrowserAgent*)
                 browserViewVisibilityNotifierBrowserAgent
    discoverFeedVisibilityBrowserAgent:
        (DiscoverFeedVisibilityBrowserAgent*)discoverFeedVisibilityBrowserAgent
              featureEngagementTracker:(feature_engagement::Tracker*)tracker {
  self = [super init];
  if (self) {
    CHECK(identityManager);
    CHECK(accountManagerService);
    CHECK(tracker);
    _templateURLService = templateURLService;
    _defaultSearchEngine = templateURLService->GetDefaultSearchProvider();
    _URLLoader = URLLoader;
    _authService = authService;
    _accountManagerService = accountManagerService;
    _identityManager = identityManager;
    _identityObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _browserViewVisibilityNotifierBrowserAgent =
        browserViewVisibilityNotifierBrowserAgent;
    _browserViewVisibilityObserverBridge =
        std::make_unique<BrowserViewVisibilityObserverBridge>(self);
    // Listen for default search engine changes.
    _searchEngineObserver = std::make_unique<SearchEngineObserverBridge>(
        self, self.templateURLService);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
    _imageUpdater = imageUpdater;
    _discoverFeedService = discoverFeedService;
    _discoverFeedVisibilityBrowserAgent = discoverFeedVisibilityBrowserAgent;
    _prefService = prefService;
    _regionalCapabilitiesService = regionalCapabilitiesService;
    _backgroundCustomizationService = backgroundCustomizationService;
    _imageFetcherService = imageFetcherService;
    _signedInIdentity =
        _authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    _tracker = tracker;
  }
  return self;
}

#pragma mark - NewTabPageMutator

- (void)checkNewBadgeEligibility {
  // Notify the badge holdback period has been satisfied if this is not the
  // First Run, or the First Run happened longer than the holdback period.
  if (!IsFirstRun() || !IsFirstRunRecent(kFREBadgeHoldbackPeriod)) {
    _tracker->NotifyEvent(
        feature_engagement::events::kIOSFREBadgeHoldbackPeriodElapsed);
  }
}

- (void)notifyLensBadgeDisplayed {
  LogLensButtonNewBadgeShownHistogram(IOSNTPNewBadgeShownResult::kShown);

  _tracker->Dismissed(feature_engagement::kIPHiOSHomepageLensNewBadge);
}

- (void)notifyCustomizationBadgeDisplayed {
  // TODO(crbug.com/428691449): Remove once the FET migration for 'new' badges
  // is fully validated.
  base::RecordAction(
      base::UserMetricsAction(kNTPCustomizationNewBadgeShownAction));

  _tracker->Dismissed(feature_engagement::kIPHiOSHomepageCustomizationNewBadge);
}

- (BOOL)isFeedHeaderVisible {
  return _discoverFeedVisibilityBrowserAgent->ShouldBeVisible();
}

- (void)setUp {
  self.templateURLService->Load();
  [self updateModuleVisibilityForConsumer];
  SearchEngineLogoState logoState =
      search::DefaultSearchProviderIsGoogle(self.templateURLService)
          ? SearchEngineLogoState::kLogo
          : SearchEngineLogoState::kNone;
  [self.headerConsumer setSearchEngineLogoState:logoState];
  [self.headerConsumer
      setVoiceSearchIsEnabled:ios::provider::IsVoiceSearchEnabled()];

  const TemplateURL* defaultSearchEngine =
      self.templateURLService->GetDefaultSearchProvider();
  NSString* dseName =
      defaultSearchEngine
          ? [NSString
                cr_fromString16:defaultSearchEngine
                                    ->AdjustedShortNameForLocaleDirection()]
          : @"";
  [self.headerConsumer setDefaultSearchEngineName:dseName];

  if (self.placeholderService) {
    // The DSE icon might have already been fetched. In this case, no updated
    // will be delivered. Therefore we should query the cache, as the icon store
    // might have already been updated.
    UIImage* fetchedIcon =
        self.placeholderService->GetDefaultSearchEngineIcon(kIconPointSize);
    if (fetchedIcon) {
      [self.headerConsumer setDefaultSearchEngineImage:fetchedIcon];
    }
  }

  [self updateAccountImage];
  [self updateAccountErrorBadge];
  [self startObservingPrefs];
  _browserViewVisibilityNotifierBrowserAgent->AddObserver(
      _browserViewVisibilityObserverBridge.get());
  _discoverFeedVisibilityBrowserAgent->AddObserver(self.feedVisibilityObserver);
  if (IsNTPBackgroundCustomizationEnabled()) {
    _backgroundCustomizationServiceObserverBridge =
        std::make_unique<HomeBackgroundCustomizationServiceObserverBridge>(
            _backgroundCustomizationService, self);
  }
  [self updateAIMAvailability];
}

- (void)shutdown {
  _browserViewVisibilityNotifierBrowserAgent->RemoveObserver(
      _browserViewVisibilityObserverBridge.get());
  _discoverFeedVisibilityBrowserAgent->RemoveObserver(
      self.feedVisibilityObserver);
  _searchEngineObserver.reset();
  _identityObserverBridge.reset();
  _browserViewVisibilityObserverBridge.reset();
  self.accountManagerService = nil;
  self.discoverFeedService = nullptr;
  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _prefService = nullptr;
  _syncObserver.reset();
  _syncService = nullptr;
  _regionalCapabilitiesService = nullptr;
  _identityManager = nullptr;
  self.feedControlDelegate = nil;
  _backgroundCustomizationServiceObserverBridge = nullptr;
  _backgroundCustomizationService = nullptr;
  _imageFetcherService = nullptr;
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate) ||
      base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    self.placeholderService = nullptr;
  }
}

- (void)saveNTPStateForWebState:(web::WebState*)webState {
  NewTabPageState* NTPState = [[NewTabPageState alloc]
      initWithScrollPosition:self.scrollPositionToSave
                selectedFeed:[self.feedControlDelegate selectedFeed]
       followingFeedSortType:[self.feedControlDelegate followingFeedSortType]];
  self.feedMetricsRecorder.NTPState = NTPState;
  NewTabPageTabHelper::FromWebState(webState)->SetNTPState(NTPState);
}

- (void)restoreNTPStateForWebState:(web::WebState*)webState {
  NewTabPageState* NTPState =
      NewTabPageTabHelper::FromWebState(webState)->GetNTPState();
  self.feedMetricsRecorder.NTPState = NTPState;
  if ([self.feedControlDelegate isFollowingFeedAvailable]) {
    [self.NTPContentDelegate updateForSelectedFeed:NTPState.selectedFeed];
  }

  if (NTPState.shouldScrollToTopOfFeed) {
    [self.consumer restoreScrollPositionToTopOfFeed];
    // Prevent next NTP from being scrolled to the top of feed.
    NTPState.shouldScrollToTopOfFeed = NO;
    NewTabPageTabHelper::FromWebState(webState)->SetNTPState(NTPState);
  } else {
    [self.consumer restoreScrollPosition:NTPState.scrollPosition];
  }
}

- (void)setPlaceholderService:(PlaceholderService*)placeholderService {
  CHECK(base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate) ||
        base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2));

  _placeholderService = placeholderService;

  if (!placeholderService) {
    _placeholderServiceObserver.reset();
    return;
  }

  _placeholderServiceObserver =
      std::make_unique<PlaceholderServiceObserverBridge>(self,
                                                         placeholderService);
}

- (void)updateBackground {
  std::optional<sync_pb::NtpCustomBackground> background =
      _backgroundCustomizationService->GetCurrentCustomBackground();

  std::optional<sync_pb::UserColorTheme> colorTheme =
      _backgroundCustomizationService->GetCurrentColorTheme();

  CustomUITraitAccessor* traitAccessor = [[CustomUITraitAccessor alloc]
      initWithMutableTraits:self.consumer.traitOverrides];

  if (colorTheme && colorTheme->color()) {
    // Sets the New Tab Page trait to a color palette generated from the current
    // theme.
    NewTabPageColorPalette* colorPalette = CreateColorPaletteFromSeedColor(
        skia::UIColorFromSkColor(colorTheme->color()),
        ProtoEnumToSchemeVariant(colorTheme->browser_color_variant()));

    [traitAccessor setObjectForNewTabPageTrait:colorPalette];
    [self.consumer setBackgroundImage:nil];
    [self.headerConsumer updateLogoColor:colorPalette.tintColor];
    [traitAccessor setBoolForNewTabPageImageBackgroundTrait:NO];
    return;
  }

  // Clears the color palette associated with the New Tab Page trait,
  // reverting to the default colors defined by the trait.
  [traitAccessor setObjectForNewTabPageTrait:[NewTabPageTrait defaultValue]];

  [traitAccessor
      setBoolForNewTabPageImageBackgroundTrait:background.has_value()];

  if (!background) {
    [self.consumer setBackgroundImage:nil];
    [self.headerConsumer updateLogoColor:nil];
    return;
  }

  GURL imageURL = GURL(background->url());

  image_fetcher::ImageFetcher* imageFetcher =
      _imageFetcherService->GetImageFetcher(
          image_fetcher::ImageFetcherConfig::kDiskCacheOnly);

  __weak __typeof(self) weakSelf = self;
  imageFetcher->FetchImage(
      imageURL,
      base::BindOnce(^(const gfx::Image& image,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf handleBackgroundImageFetch:image];
      }),
      // TODO (crbug.com/417234848): Add annotation.
      image_fetcher::ImageFetcherParams(NO_TRAFFIC_ANNOTATION_YET, "Test"));
}

#pragma mark - BrowserViewVisibilityObserving

- (void)browserViewDidChangeToVisibilityState:
            (BrowserViewVisibilityState)currentState
                                    fromState:(BrowserViewVisibilityState)
                                                  previousState {
  if (self.discoverFeedService && self.NTPVisible &&
      [self isFeedHeaderVisible]) {
    self.discoverFeedService->UpdateFeedViewVisibilityState(
        self.contentCollectionView, currentState, previousState);
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  const TemplateURL* updatedDefaultSearchEngine =
      self.templateURLService->GetDefaultSearchProvider();
  if (_defaultSearchEngine == updatedDefaultSearchEngine) {
    return;
  }
  _defaultSearchEngine = updatedDefaultSearchEngine;
  // AIM availability must be updated before default search engine.
  [self updateAIMAvailability];
  SearchEngineLogoState logoState =
      search::DefaultSearchProviderIsGoogle(self.templateURLService)
          ? SearchEngineLogoState::kLogo
          : SearchEngineLogoState::kNone;
  [self.headerConsumer setSearchEngineLogoState:logoState];
  [self.feedControlDelegate updateFeedForDefaultSearchEngineChanged];

  NSString* dseName =
      updatedDefaultSearchEngine
          ? [NSString
                cr_fromString16:updatedDefaultSearchEngine
                                    ->AdjustedShortNameForLocaleDirection()]
          : @"";
  [self.headerConsumer setDefaultSearchEngineName:dseName];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfPrimaryAccountChanges {
  _signedInIdentity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  [self updateAccountImage];
  [self updateAccountErrorBadge];
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (info.gaia != GaiaId(_signedInIdentity.gaiaID)) {
    return;
  }
  [self updateAccountImage];
  [self updateAccountErrorBadge];
}

#pragma mark - PlaceholderServiceObserving

- (void)placeholderImageUpdated {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    return;
  }

  // Show Default Search Engine favicon.
  // Remember what is the Default Search Engine provider that the icon is
  // for, in case the user changes Default Search Engine while this is being
  // loaded.
  __weak __typeof(self) weakSelf = self;
  if (self.placeholderService) {
    self.placeholderService->FetchDefaultSearchEngineIcon(
        kIconPointSize, base::BindRepeating(^(UIImage* image) {
          [weakSelf.headerConsumer setDefaultSearchEngineImage:image];
        }));
  }
}

- (void)placeholderServiceShuttingDown:(PlaceholderService*)service {
  // Removes observation.
  self.placeholderService = nil;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // Handle customization prefs
  if (preferenceName == prefs::kHomeCustomizationMostVisitedEnabled ||
      preferenceName == prefs::kHomeCustomizationMagicStackEnabled) {
    [self updateModuleVisibilityForConsumer];
    [self.NTPContentDelegate updateModuleVisibility];
  }
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self updateAccountErrorBadge];
}

#pragma mark - HomeBackgroundCustomizationServiceObserving

- (void)onBackgroundChanged {
  [self updateBackground];
}

#pragma mark - Private

- (void)updateAIMAvailability {
  BOOL aimAllowed = IsAIMAvailable(_prefService, self.templateURLService);
  [self.consumer setAIMAllowed:aimAllowed];
  [self.headerConsumer setAIMAllowed:aimAllowed];
}

// Fetches and update user's avatar on NTP, or use default avatar if user is
// not signed in.
- (void)updateAccountImage {
  // Fetches user's identity from Authentication Service.
  if (_signedInIdentity) {
    // Only show an avatar if the user is signed in.
    UIImage* image = self.accountManagerService->GetIdentityAvatarWithIdentity(
        _signedInIdentity, IdentityAvatarSize::SmallSize);
    [self.imageUpdater updateAccountImage:image
                                     name:_signedInIdentity.userFullName
                                    email:_signedInIdentity.userEmail];
  } else {
    [self.imageUpdater setSignedOutAccountImage];
    signin_metrics::LogSignInOffered(
        signin_metrics::AccessPoint::kNtpSignedOutIcon,
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  }
}

// Opens web page for a menu item in the NTP.
- (void)openMenuItemWebPage:(GURL)URL {
  _URLLoader->Load(UrlLoadParams::InCurrentTab(URL));
  // TODO(crbug.com/40693626): Add metrics.
}

// Updates the consumer with the current visibility of the NTP modules.
- (void)updateModuleVisibilityForConsumer {
  self.consumer.mostVisitedVisible =
      _prefService->GetBoolean(prefs::kHomeCustomizationMostVisitedEnabled);
  self.consumer.magicStackVisible =
      _prefService->GetBoolean(prefs::kHomeCustomizationMagicStackEnabled);
}

// Starts observing some prefs.
- (void)startObservingPrefs {
  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(_prefService);
  _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

  // Observe customization prefs.
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kHomeCustomizationMostVisitedEnabled, _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kHomeCustomizationMagicStackEnabled, _prefChangeRegistrar.get());
}

- (void)updateAccountErrorBadge {
  if (!base::FeatureList::IsEnabled(
          switches::kEnableErrorBadgeOnIdentityDisc) &&
      !base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
    return;
  }
  BOOL primaryIdentityHasError =
      _signedInIdentity && _syncService->GetUserActionableError() !=
                               syncer::SyncService::UserActionableError::kNone;
  [self.headerConsumer
      updateADPBadgeWithErrorFound:primaryIdentityHasError
                              name:_signedInIdentity.userFullName
                             email:_signedInIdentity.userEmail];
}

// Helper method to handle the image response after fetching the background
// image for the new tab page.
- (void)handleBackgroundImageFetch:(const gfx::Image&)image {
  [self.consumer setBackgroundImage:image.ToUIImage()];
  [self.headerConsumer updateLogoColor:UIColor.whiteColor];
}

@end
