// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mediator.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/cancelable_callback.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/image_fetcher/core/request_metadata.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
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
#import "ios/chrome/browser/home_customization/coordinator/home_customization_data_conversion.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer_bridge.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
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
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
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
#import "ui/base/device_form_factor.h"
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

// Key for Image Fetcher UMA metrics.
constexpr char kImageFetcherUmaClient[] = "NtpBackground";

// NetworkTrafficAnnotationTag for fetching ntp background image from Google
// server.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ntp_background_custom_image",
                                        R"(
        semantics {
        sender: "NtpBackground"
        description:
            "Sends a request to a Google server to load the ntp's custom "
            "background."
        trigger:
            "A request will be sent when the user opens a new NTP and has a "
            "custom background."
        data: "Only image url, no user data"
        destination: GOOGLE_OWNED_SERVICE
        }
        policy {
        cookies_allowed: NO
        setting:
            "This feature cannot be disabled by settings. However, the "
            "request will only be made if the user has a custom NTP background."
        chrome_policy: {
          NTPCustomBackgroundEnabled {
            NTPCustomBackgroundEnabled: false
          }
        }
        }
        )");

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
  // AIM eligibility service.
  raw_ptr<AimEligibilityService> _aimEligibilityService;
  // AIM eligibility subscription.
  base::CallbackListSubscription _aimEligibilitySubscription;
  // Whether AIM is currently allowed.
  BOOL _isAIMAllowed;
  // Listen for default search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  // Observes changes in identity and updates the Identity Disc.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // Observes changes of the browser view visibility state.
  raw_ptr<BrowserViewVisibilityNotifierBrowserAgent, DanglingUntriaged>
      _browserViewVisibilityNotifierBrowserAgent;
  // Observes changes of the feed visibility state.
  raw_ptr<DiscoverFeedVisibilityBrowserAgent, DanglingUntriaged>
      _discoverFeedVisibilityBrowserAgent;
  std::unique_ptr<BrowserViewVisibilityObserverBridge>
      _browserViewVisibilityObserverBridge;
  // Used to load URLs.
  raw_ptr<UrlLoadingBrowserAgent, DanglingUntriaged> _URLLoader;
  raw_ptr<PrefService> _prefService;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // The current default search engine.
  raw_ptr<const TemplateURL, DanglingUntriaged> _defaultSearchEngine;
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
  raw_ptr<UserUploadedImageManager, DanglingUntriaged>
      _userUploadedImageManager;
  // Observer to keep track of the syncing status.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  raw_ptr<signin::IdentityManager> _identityManager;
  id<SystemIdentity> _signedInIdentity;
  std::unique_ptr<PlaceholderServiceObserverBridge> _placeholderServiceObserver;
  // Feature engagement tracker for handling "new" badge IPH.
  raw_ptr<feature_engagement::Tracker, DanglingUntriaged> _tracker;
  // Tracks whether the NTP was ever in landscape.
  BOOL _wasNTPInLandscape;
  // Whether the mediator has been set up.
  BOOL _mediatorSetUp;
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
              userUploadedImageManager:
                  (UserUploadedImageManager*)userUploadedImageManager
         browserViewVisibilityNotifier:
             (BrowserViewVisibilityNotifierBrowserAgent*)
                 browserViewVisibilityNotifierBrowserAgent
    discoverFeedVisibilityBrowserAgent:
        (DiscoverFeedVisibilityBrowserAgent*)discoverFeedVisibilityBrowserAgent
              featureEngagementTracker:(feature_engagement::Tracker*)tracker
                 aimEligibilityService:
                     (AimEligibilityService*)aimEligibilityService {
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
    _userUploadedImageManager = userUploadedImageManager;
    _signedInIdentity =
        _authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
    _tracker = tracker;
    _aimEligibilityService = aimEligibilityService;
    if (_aimEligibilityService) {
      __weak __typeof(self) weakSelf = self;
      _aimEligibilitySubscription =
          _aimEligibilityService->RegisterEligibilityChangedCallback(
              base::BindRepeating(^(void) {
                [weakSelf updateAIMAvailability];
              }));
    }
  }
  return self;
}

#pragma mark - NewTabPageMutator

- (void)notifyNtpDisplayedInLandscape {
  _wasNTPInLandscape = YES;
}

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
  _mediatorSetUp = YES;
}

- (void)shutdown {
  _mediatorSetUp = NO;
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
  _aimEligibilitySubscription = {};
  _aimEligibilityService = nullptr;
  _isAIMAllowed = NO;
  self.feedControlDelegate = nil;
  _backgroundCustomizationServiceObserverBridge = nullptr;
  _backgroundCustomizationService = nullptr;
  _imageFetcherService = nullptr;
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate) ||
      base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    self.placeholderService = nullptr;
  }
  base::UmaHistogramBoolean("IOS.NTP.LandscapeMode", _wasNTPInLandscape);
}

- (void)saveNTPScrollPositionForWebState:(web::WebState*)webState {
  NewTabPageTabHelper::FromWebState(webState)->SetNTPScrollPosition(
      self.scrollPositionToSave);
}

- (void)restoreNTPScrollPositionForWebState:(web::WebState*)webState {
  [self.consumer
      restoreScrollPosition:NewTabPageTabHelper::FromWebState(webState)
                                ->GetNTPScrollPosition()];
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
  [self updateBackgroundForInitialLoad:YES];
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
  if (info.gaia != _signedInIdentity.gaiaId) {
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
  // Handle customization prefs.
  if (preferenceName == ntp_tiles::prefs::kMostVisitedHomeModuleEnabled ||
      preferenceName == ntp_tiles::prefs::kMagicStackHomeModuleEnabled) {
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
  [self updateBackgroundForInitialLoad:NO];
}

#pragma mark - Private

- (void)updateAIMAvailability {
  BOOL aimAllowed = NO;
  if (_aimEligibilityService) {
    const BOOL allowedOnDevice =
        ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE ||
        IsAIMNTPEntrypointTabletEnabled();
    aimAllowed = _aimEligibilityService->IsAimEligible() && allowedOnDevice;
  }

  [self.consumer setAIMAllowed:aimAllowed];
  [self.headerConsumer setAIMAllowed:aimAllowed];

  if (aimAllowed == _isAIMAllowed) {
    return;
  }
  _isAIMAllowed = aimAllowed;
  // Only update the modules if the mediator has already been set up.
  if (IsAIMEligibilityRefreshNTPModulesEnabled() && _mediatorSetUp) {
    [self.NTPContentDelegate updateModuleVisibility];
  }
}

// Fetches and update user's avatar on NTP, or use default avatar if user is
// not signed in.
- (void)updateAccountImage {
  // Fetches user's identity from Authentication Service.
  if (_signedInIdentity) {
    // Only show an avatar if the user is signed in.
    UIImage* image =
        GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
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
      _prefService->GetBoolean(ntp_tiles::prefs::kMostVisitedHomeModuleEnabled);
  self.consumer.magicStackVisible =
      _prefService->GetBoolean(ntp_tiles::prefs::kMagicStackHomeModuleEnabled);
}

// Starts observing some prefs.
- (void)startObservingPrefs {
  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(_prefService);
  _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

  // Observe customization prefs.
  _prefObserverBridge->ObserveChangesForPreference(
      ntp_tiles::prefs::kMostVisitedHomeModuleEnabled,
      _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
      _prefChangeRegistrar.get());
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
  [self.consumer setBackgroundImage:image.ToUIImage() framingCoordinates:nil];
}

// Helper method to handle displaying a user-uploaded background image
// with the specified framing coordinates.
- (void)handleUserUploadedImage:(UIImage*)image
             framingCoordinates:
                 (HomeCustomizationFramingCoordinates*)framingCoordinates {
  if (!image) {
    // Clear the corrupted data.
    _backgroundCustomizationService->ClearCurrentUserUploadedBackground();
    _backgroundCustomizationService->StoreCurrentTheme();
    [self.consumer setBackgroundImage:nil framingCoordinates:nil];
    return;
  }

  [self.consumer setBackgroundImage:image
                 framingCoordinates:framingCoordinates];
}

// Updates the background based on the current customization settings.
// `initialLoad` is YES if this is the first time the background is being set.
- (void)updateBackgroundForInitialLoad:(BOOL)initialLoad {
  CustomUITraitAccessor* traitAccessor = [[CustomUITraitAccessor alloc]
      initWithMutableTraits:self.consumer.traitOverrides];

  std::optional<HomeCustomBackground> customBackground =
      _backgroundCustomizationService->GetCurrentCustomBackground();

  if (customBackground) {
    if (std::holds_alternative<sync_pb::NtpCustomBackground>(
            customBackground.value())) {
      sync_pb::NtpCustomBackground background =
          std::get<sync_pb::NtpCustomBackground>(customBackground.value());

      GURL imageURL = GURL(background.url());
      GURL thumbnailURL = AddOptionsToImageURL(
          RemoveOptionsFromImageURL(imageURL.spec()).spec(),
          GetThumbnailImageOptions());

      image_fetcher::ImageFetcher* imageFetcher =
          _imageFetcherService->GetImageFetcher(
              image_fetcher::ImageFetcherConfig::kDiskCacheOnly);

      __weak __typeof(self) weakSelf = self;

      auto cancelable_thumbnail_callback =
          std::make_shared<base::CancelableOnceCallback<void(
              const gfx::Image&, const image_fetcher::RequestMetadata&)>>();

      cancelable_thumbnail_callback->Reset(
          base::BindOnce(^(const gfx::Image& image,
                           const image_fetcher::RequestMetadata& metadata) {
            if (!image.IsEmpty()) {
              // Temporarily sets the thumbnail as the background until the
              // high-resolution image is loaded.
              [weakSelf handleBackgroundImageFetch:image];
              return;
            }
          }));

      // Retrieving the thumbnail URL should hit the cache, so it returns almost
      // instantly.
      imageFetcher->FetchImage(thumbnailURL,
                               cancelable_thumbnail_callback->callback(),
                               image_fetcher::ImageFetcherParams(
                                   kTrafficAnnotation, kImageFetcherUmaClient));

      imageFetcher->FetchImage(
          imageURL,
          base::BindOnce(^(const gfx::Image& image,
                           const image_fetcher::RequestMetadata& metadata) {
            // Cancel the thumbnail URL fetch if the high-resolution fetch
            // finished first.
            if (cancelable_thumbnail_callback) {
              cancelable_thumbnail_callback->Cancel();
            }
            if (!image.IsEmpty()) {
              [weakSelf handleBackgroundImageFetch:image];
              [traitAccessor setBoolForNewTabPageImageBackgroundTrait:YES];
              [traitAccessor
                  setObjectForNewTabPageTrait:[NewTabPageTrait defaultValue]];
            } else {
              base::UmaHistogramSparse(
                  "IOS.HomeCustomization.Background.Ntp.ImageDownloadErrorCode",
                  metadata.http_response_code);
            }
          }),
          image_fetcher::ImageFetcherParams(kTrafficAnnotation,
                                            kImageFetcherUmaClient));
      if (initialLoad) {
        base::UmaHistogramEnumeration(
            "IOS.HomeCustomization.Background.Ntp.Loaded",
            HomeCustomizationBackgroundStyle::kPreset);
      }
    } else {
      HomeUserUploadedBackground userBackground =
          std::get<HomeUserUploadedBackground>(customBackground.value());
      HomeCustomizationFramingCoordinates* framingCoordinates =
          HomeCustomizationFramingCoordinatesFromFramingCoordinates(
              userBackground.framing_coordinates);

      __weak __typeof(self) weakSelf = self;
      _userUploadedImageManager->LoadUserUploadedImage(
          base::FilePath(userBackground.image_path),
          base::BindOnce(^(UIImage* image, UserUploadedImageError error) {
            [weakSelf handleUserUploadedImage:image
                           framingCoordinates:framingCoordinates];
            [traitAccessor setBoolForNewTabPageImageBackgroundTrait:YES];
            [traitAccessor
                setObjectForNewTabPageTrait:[NewTabPageTrait defaultValue]];
            if (!image) {
              base::UmaHistogramEnumeration("IOS.HomeCustomization.Background."
                                            "Ntp.ImageUserUploadedFetchError",
                                            error);
            }
          }));
      if (initialLoad) {
        base::UmaHistogramEnumeration(
            "IOS.HomeCustomization.Background.Ntp.Loaded",
            HomeCustomizationBackgroundStyle::kUserUploaded);
      }
    }
    return;
  } else {
    [self.consumer setBackgroundImage:nil framingCoordinates:nil];
  }

  std::optional<sync_pb::UserColorTheme> colorTheme =
      _backgroundCustomizationService->GetCurrentColorTheme();

  if (colorTheme && colorTheme->color()) {
    // Sets the New Tab Page trait to a color palette generated from the current
    // theme.
    NewTabPageColorPalette* colorPalette = CreateColorPaletteFromSeedColor(
        skia::UIColorFromSkColor(colorTheme->color()),
        ProtoEnumToSchemeVariant(colorTheme->browser_color_variant()));

    [traitAccessor setObjectForNewTabPageTrait:colorPalette];
    [traitAccessor setBoolForNewTabPageImageBackgroundTrait:NO];
    if (initialLoad) {
      base::UmaHistogramEnumeration(
          "IOS.HomeCustomization.Background.Ntp.Loaded",
          HomeCustomizationBackgroundStyle::kColor);
    }
    return;
  }

  // Clears the color palette associated with the New Tab Page trait,
  // reverting to the default colors defined by the trait.
  [traitAccessor setObjectForNewTabPageTrait:[NewTabPageTrait defaultValue]];
  [traitAccessor setBoolForNewTabPageImageBackgroundTrait:NO];
  base::UmaHistogramEnumeration("IOS.HomeCustomization.Background.Ntp.Loaded",
                                HomeCustomizationBackgroundStyle::kDefault);
}

@end
