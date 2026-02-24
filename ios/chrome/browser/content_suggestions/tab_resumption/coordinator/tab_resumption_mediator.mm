// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/tab_resumption/coordinator/tab_resumption_mediator.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/command_line.h"
#import "base/containers/flat_set.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/commerce/core/commerce_constants.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/commerce_types.h"
#import "components/commerce/core/commerce_utils.h"
#import "components/commerce/core/price_tracking_utils.h"
#import "components/commerce/core/proto/price_tracking.pb.h"
#import "components/commerce/core/shopping_service.h"
#import "components/ntp_tiles/pref_names.h"
#import "components/optimization_guide/core/hints/optimization_guide_decision.h"
#import "components/optimization_guide/proto/common_types.pb.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/page_image_service/image_service.h"
#import "components/page_image_service/mojom/page_image_service.mojom.h"
#import "components/payments/core/currency_formatter.h"
#import "components/sessions/core/session_id.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/sync_sessions/open_tabs_ui_delegate.h"
#import "components/sync_sessions/session_sync_service.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_suggestions/impression_limits/model/impression_limit_service.h"
#import "ios/chrome/browser/content_suggestions/model/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/shop_card/public/shop_card_constants.h"
#import "ios/chrome/browser/content_suggestions/shop_card/ui/shop_card_data.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/coordinator/tab_resumption_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/public/tab_resumption_constants.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_commands.h"
#import "ios/chrome/browser/content_suggestions/tab_resumption/ui/tab_resumption_config.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/intents/model/intents_donation_helper.h"
#import "ios/chrome/browser/metrics/model/new_tab_page_uma.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_actions_delegate.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/page_image/model/page_image_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/sessions/model/session_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/price_tracked_items_commands.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_browser_agent.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_recent_tab_removal_observer_bridge.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_util.h"
#import "ios/chrome/browser/sync/model/session_sync_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/synced_sessions/model/distant_session.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_utils.h"
#import "ios/chrome/browser/tabs/model/tab_sync_util.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// A command line flag to override the default sync threshold.
const char kTabResumptionThresholdParameter[] = "tab-resumption-sync-threshold";

const base::TimeDelta TabResumptionForXDevicesTimeThreshold() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string paramter =
      command_line->GetSwitchValueASCII(kTabResumptionThresholdParameter);
  int threshold = 0;
  if (!base::StringToInt(kTabResumptionThresholdParameter, &threshold)) {
    threshold = 12 * 3600;
  }
  return base::Seconds(threshold);
}

// Whether the item should be displayed immediately (before fetching an image).
bool ShouldShowItemImmediately() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kTabResumptionShowItemImmediately);
}

enum class ShopCardTrackItemResult {
  kTrackSuccess,
  kTrackSuccesNoNotification,
  kTrackError,
};

// Salient images should come from gstatic.com.
const char kGStatic[] = ".gstatic.com";

NSString* GetFormattedPrice(payments::CurrencyFormatter* formatter,
                            long price_micros) {
  float price = static_cast<float>(price_micros) /
                static_cast<float>(commerce::kToMicroCurrency);
  formatter->SetMaxFractionalDigits(price >= 10.0 ? 0 : 2);
  return base::SysUTF16ToNSString(
      formatter->Format(base::NumberToString(price)));
}

PriceDrop GetPriceDrop(payments::CurrencyFormatter* formatter,
                       long current_price_micros,
                       long previous_price_micros) {
  PriceDrop price_drop;
  price_drop.current_price = GetFormattedPrice(formatter, current_price_micros);
  price_drop.previous_price =
      GetFormattedPrice(formatter, previous_price_micros);
  return price_drop;
}

bool HasPriceDropDataForTabResumption(
    const std::optional<const commerce::PriceTrackingData>&
        price_tracking_data) {
  return price_tracking_data.has_value() &&
         price_tracking_data->has_product_update() &&
         price_tracking_data->product_update().has_old_price() &&
         price_tracking_data->product_update().has_new_price() &&
         price_tracking_data->product_update()
             .old_price()
             .has_currency_code() &&
         price_tracking_data->product_update()
             .new_price()
             .has_currency_code() &&
         price_tracking_data->product_update().old_price().currency_code() ==
             price_tracking_data->product_update()
                 .new_price()
                 .currency_code() &&
         price_tracking_data->has_buyable_product() &&
         price_tracking_data->buyable_product().has_title();
}

bool HasCurrentPriceDataForTabResumption(
    const std::optional<const commerce::PriceTrackingData>&
        price_tracking_data) {
  return price_tracking_data.has_value() &&
         price_tracking_data->has_buyable_product() &&
         price_tracking_data->buyable_product().has_current_price() &&
         price_tracking_data->buyable_product()
             .current_price()
             .has_currency_code() &&
         price_tracking_data->buyable_product()
             .current_price()
             .has_amount_micros();
}

// A Product Detail Page is price trackable if it has a cluster ID.
bool IsPriceTrackable(const std::optional<const commerce::PriceTrackingData>&
                          price_tracking_data) {
  return price_tracking_data.has_value() &&
         price_tracking_data->has_buyable_product() &&
         price_tracking_data->buyable_product().has_product_cluster_id();
}

std::u16string GetHostnameFromGURL(const GURL& url) {
  return url_formatter::
      FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(url);
}

void AddProductImageIfApplicable(
    const commerce::PriceTrackingData& price_tracking_data,
    TabResumptionConfig* config) {
  if (price_tracking_data.has_buyable_product() &&
      price_tracking_data.buyable_product().has_image_url()) {
    config.shopCardData.productImageURL =
        price_tracking_data.buyable_product().image_url();
  }
}

void ConfigureTabResumptionItemForShopCard(
    const base::flat_map<
        optimization_guide::proto::OptimizationType,
        optimization_guide::OptimizationGuideDecisionWithMetadata>& decisions,
    TabResumptionConfig* config,
    const GURL& url) {
  auto iter = decisions.find(optimization_guide::proto::PRICE_TRACKING);
  if (iter == decisions.end()) {
    return;
  }

  optimization_guide::OptimizationGuideDecisionWithMetadata
      decisionWithMetadata = iter->second;
  if (decisionWithMetadata.decision !=
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return;
  }
  const std::optional<const commerce::PriceTrackingData>& price_tracking_data =
      decisionWithMetadata.metadata
          .ParsedMetadata<commerce::PriceTrackingData>();

  if ((commerce::kShopCardVariation.Get().contains(commerce::kShopCardArm3) ||
       commerce::kShopCardVariation.Get() == commerce::kShopCardArm6) &&
      HasPriceDropDataForTabResumption(price_tracking_data)) {
    config.shopCardData = [[ShopCardData alloc] init];
    config.shopCardData.shopCardItemType = ShopCardItemType::kPriceDropOnTab;

    std::unique_ptr<payments::CurrencyFormatter> formatter =
        std::make_unique<payments::CurrencyFormatter>(
            price_tracking_data->product_update().new_price().currency_code(),
            GetApplicationContext()->GetApplicationLocaleStorage()->Get());
    config.shopCardData.priceDrop = GetPriceDrop(
        formatter.get(),
        price_tracking_data->product_update().new_price().amount_micros(),
        price_tracking_data->product_update().old_price().amount_micros());
    AddProductImageIfApplicable(price_tracking_data.value(), config);
    config.shopCardData.accessibilityString = l10n_util::GetNSStringF(
        IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_PRICE_DROP_OPEN_TABS_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(config.shopCardData.priceDrop->previous_price),
        base::SysNSStringToUTF16(config.shopCardData.priceDrop->current_price),
        base::UTF8ToUTF16(price_tracking_data->buyable_product().title()),
        GetHostnameFromGURL(url));
  }

  // A URL is price trackable if it has a cluster ID.
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm4 &&
      IsPriceTrackable(price_tracking_data)) {
    config.shopCardData = [[ShopCardData alloc] init];
    config.shopCardData.shopCardItemType =
        ShopCardItemType::kPriceTrackableProductOnTab;

    std::unique_ptr<commerce::ProductInfo> info =
        commerce::OptGuideResultToProductInfo(decisionWithMetadata.metadata);
    if (info) {
      config.shopCardData.productInfo = std::move(*info);
    }

    if (HasCurrentPriceDataForTabResumption(price_tracking_data)) {
      std::unique_ptr<payments::CurrencyFormatter> formatter =
          std::make_unique<payments::CurrencyFormatter>(
              price_tracking_data->buyable_product()
                  .current_price()
                  .currency_code(),
              GetApplicationContext()->GetApplicationLocaleStorage()->Get());
      config.shopCardData.currentPrice = GetFormattedPrice(
          formatter.get(), price_tracking_data->buyable_product()
                               .current_price()
                               .amount_micros());
    }
    config.shopCardData.accessibilityString = l10n_util::GetNSStringF(
        IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_ACCESSIBILITY_LABEL,
        base::SysNSStringToUTF16(config.tabTitle),
        base::SysNSStringToUTF16(config.shopCardData.currentPrice),
        GetHostnameFromGURL(url));
    AddProductImageIfApplicable(price_tracking_data.value(), config);
  }
}

bool IsShopCardImpressionLimitsEnabled() {
  return base::FeatureList::IsEnabled(commerce::kShopCardImpressionLimits) &&
         (commerce::kShopCardVariation.Get().contains(
              commerce::kShopCardArm3) ||
          commerce::kShopCardVariation.Get() == commerce::kShopCardArm4 ||
          commerce::kShopCardVariation.Get() == commerce::kShopCardArm5);
}

int GetImpressionLimit() {
  return base::GetFieldTrialParamByFeatureAsInt(
      commerce::kTabResumptionShopCard, commerce::kShopCardMaxImpressions,
      kShopCardMaxImpressions);
}

const char* GetImpressionLimitPref() {
  if (commerce::kShopCardVariation.Get().contains(commerce::kShopCardArm3)) {
    return tab_resumption_prefs::kTabResumptionWithPriceDropUrlImpressions;
  } else if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm4) {
    return tab_resumption_prefs::kTabResumptionWithPriceTrackableUrlImpressions;
  } else if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm5) {
    return tab_resumption_prefs::kTabResumptionRegularUrlImpressions;
  }
  NOTREACHED();
}

}  // namespace

// Call through to OptimizationGuide's OnDemand API which is a restricted
// API. In order to call the private function CanApplyOptimizationOnDemand,
// a class to friend OptimizationGuideService is needed.
class TabResumptionMediatorProxy {
 public:
  // Call through to optimizationGuideService->CanApplyOptimizationOnDemand
  static void CanApplyOptimizationOnDemand(
      OptimizationGuideService* optimizationGuideService,
      const GURL& url,
      const optimization_guide::proto::OptimizationType& optimization_type,
      optimization_guide::proto::RequestContext request_context,
      optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback
          callback) {
    // It is possible for this method to be called with a null pointer as
    // the some blocks end up calling those methods after -disconnect has
    // been called on the TabResumptionMediator (which clears all the C++
    // pointers).
    //
    // See https://crbug.com/457339557 for a sample crash.
    if (optimizationGuideService) {
      optimizationGuideService->CanApplyOptimizationOnDemand(
          {url}, {optimization_type}, request_context, std::move(callback),
          std::nullopt);
    }
  }
};

@interface TabResumptionMediator () <BooleanObserver,
                                     IdentityManagerObserverBridgeDelegate,
                                     MagicStackModuleDelegate,
                                     StartSurfaceRecentTabObserving,
                                     SyncedSessionsObserver,
                                     SyncObserverModelBridge,
                                     TabResumptionCommands>
// readwrite override.
@property(nonatomic, strong, readwrite) TabResumptionConfig* itemConfig;

@end

@implementation TabResumptionMediator {
  // Last distant tab resumption item URL.
  GURL _lastDistinctItemURL;
  // Tab identifier of the last distant tab resumption item.
  std::optional<SessionID> _tabId;
  // Session tag of the last distant tab resumption item.
  std::string _sessionTag;
  BOOL _isOffTheRecord;

  // The last item that is returned by the model.
  // The URL/title will be used to not fetch again images if the same item is
  // returned twice, or to ignore update on obsolete items.
  TabResumptionConfig* _pendingItem;

  // Weak pointer to the SceneState.
  __weak SceneState* _sceneState;

  // LINT.IfChange(Dependencies)
  // The owning Browser.
  raw_ptr<Browser> _browser;
  raw_ptr<PrefService> _profilePrefs;
  // Loads favicons.
  raw_ptr<FaviconLoader> _faviconLoader;
  // Browser Agent that manages the most recent WebState.
  raw_ptr<StartSurfaceRecentTabBrowserAgent> _recentTabBrowserAgent;
  // KeyedService responsible session sync.
  raw_ptr<sync_sessions::SessionSyncService> _sessionSyncService;
  // KeyedService responsible for sync state.
  raw_ptr<syncer::SyncService> _syncService;
  raw_ptr<UrlLoadingBrowserAgent> _URLLoadingBrowserAgent;
  raw_ptr<WebStateList> _webStateList;
  // KeyedService for Salient images.
  raw_ptr<page_image_service::ImageService> _pageImageService;
  // Other KeyedServices.
  raw_ptr<OptimizationGuideService> _optimizationGuideService;
  raw_ptr<ImpressionLimitService> _impressionLimitService;
  raw_ptr<commerce::ShoppingService> _shoppingService;
  raw_ptr<bookmarks::BookmarkModel> _bookmarkModel;
  raw_ptr<PushNotificationService> _pushNotificationService;
  raw_ptr<AuthenticationService> _authenticationService;
  // LINT.ThenChange(//ios/chrome/browser/content_suggestions/tab_resumption/coordinator/tab_resumption_mediator.mm:ClearDependencies)

  // Observer bridge for mediator to listen to
  // StartSurfaceRecentTabObserverBridge.
  std::unique_ptr<StartSurfaceRecentTabObserverBridge> _startSurfaceObserver;
  std::unique_ptr<image_fetcher::ImageDataFetcher> _imageFetcher;

  std::unique_ptr<SyncObserverBridge> _syncObserverModelBridge;
  // Observer for changes to the user's identity state.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  std::unique_ptr<synced_sessions::SyncedSessionsObserverBridge>
      _syncedSessionsObserverBridge;

  // Whether the item is currently presented as Top Module by Magic Stack.
  BOOL _currentlyTopModule;
  PrefBackedBoolean* _tabResumptionDisabled;
}

- (instancetype)
          initWithLocalState:(PrefService*)localState
                 prefService:(PrefService*)prefService
             identityManager:(signin::IdentityManager*)identityManager
                     browser:(Browser*)browser
    optimizationGuideService:(OptimizationGuideService*)optimizationGuideService
      impressionLimitService:(ImpressionLimitService*)impressionLimitService
             shoppingService:(commerce::ShoppingService*)shoppingService
               bookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
     pushNotificationService:(PushNotificationService*)pushNotificationService
       authenticationService:(AuthenticationService*)authenticationService {
  self = [super init];
  if (self) {
    _profilePrefs = prefService;
    _browser = browser;
    _tabId = SessionID::InvalidValue();
    _sceneState = _browser->GetSceneState();
    _webStateList = _browser->GetWebStateList();
    _isOffTheRecord = _browser->GetProfile()->IsOffTheRecord();

    _tabResumptionDisabled = [[PrefBackedBoolean alloc]
        initWithPrefService:_profilePrefs
                   prefName:ntp_tiles::prefs::kTabResumptionHomeModuleEnabled];
    [_tabResumptionDisabled setObserver:self];

    ProfileIOS* profile = _browser->GetProfile();
    _sessionSyncService = SessionSyncServiceFactory::GetForProfile(profile);
    _syncService = SyncServiceFactory::GetForProfile(profile);
    _faviconLoader = IOSChromeFaviconLoaderFactory::GetForProfile(profile);
    _recentTabBrowserAgent =
        StartSurfaceRecentTabBrowserAgent::FromBrowser(_browser);
    _URLLoadingBrowserAgent = UrlLoadingBrowserAgent::FromBrowser(_browser);
    _startSurfaceObserver =
        std::make_unique<StartSurfaceRecentTabObserverBridge>(self);
    StartSurfaceRecentTabBrowserAgent::FromBrowser(_browser)->AddObserver(
        _startSurfaceObserver.get());
    _pageImageService = PageImageServiceFactory::GetForProfile(profile);
    _imageFetcher = std::make_unique<image_fetcher::ImageDataFetcher>(
        profile->GetSharedURLLoaderFactory());
    _syncedSessionsObserverBridge =
        std::make_unique<synced_sessions::SyncedSessionsObserverBridge>(
            self, _sessionSyncService);

    _syncObserverModelBridge =
        std::make_unique<SyncObserverBridge>(self, _syncService);
    _identityManagerObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    if (optimizationGuideService) {
      _optimizationGuideService = optimizationGuideService;
      _optimizationGuideService->RegisterOptimizationTypes(
          {optimization_guide::proto::PRICE_TRACKING});
    }
    _impressionLimitService = impressionLimitService;
    _shoppingService = shoppingService;
    _bookmarkModel = bookmarkModel;
    _pushNotificationService = pushNotificationService;
    _authenticationService = authenticationService;
  }
  return self;
}

- (void)disconnect {
  _syncedSessionsObserverBridge.reset();
  if (_startSurfaceObserver) {
    _recentTabBrowserAgent->RemoveObserver(_startSurfaceObserver.get());
    _startSurfaceObserver.reset();
  }
  _syncObserverModelBridge.reset();
  _identityManagerObserverBridge.reset();
  [_tabResumptionDisabled stop];
  [_tabResumptionDisabled setObserver:nil];
  _tabResumptionDisabled = nil;

  // LINT.IfChange(ClearDependencies)
  // Clear all pointers to C++ services.
  _browser = nullptr;
  _profilePrefs = nullptr;
  _faviconLoader = nullptr;
  _recentTabBrowserAgent = nullptr;
  _sessionSyncService = nullptr;
  _syncService = nullptr;
  _URLLoadingBrowserAgent = nullptr;
  _webStateList = nullptr;
  _pageImageService = nullptr;
  _optimizationGuideService = nullptr;
  _impressionLimitService = nullptr;
  _shoppingService = nullptr;
  _bookmarkModel = nullptr;
  _pushNotificationService = nullptr;
  _authenticationService = nullptr;
  // LINT.ThenChange(//ios/chrome/browser/content_suggestions/tab_resumption/coordinator/tab_resumption_mediator.mm:Dependencies)
}

#pragma mark - Public methods

- (void)openTabResumptionItem:(TabResumptionConfig*)config {
  [self.contentSuggestionsMetricsRecorder
      recordTabResumptionTabOpened:config.shopCardData];
  tab_resumption_prefs::SetTabResumptionLastOpenedTabURL(config.tabURL,
                                                         _profilePrefs);
  [self.delegate logMagicStackEngagementForType:ContentSuggestionsModuleType::
                                                    kTabResumption];

  NSUInteger index = [self.delegate
      indexForMagicStackModule:ContentSuggestionsModuleType::kTabResumption];

  switch (config.itemType) {
    case TabResumptionItemType::kLastSyncedTab:
      [self.NTPActionsDelegate distantTabResumptionOpenedAtIndex:index];
      [self openDistantTab:config];
      break;
    case TabResumptionItemType::kMostRecentTab: {
      [self.NTPActionsDelegate recentTabTileOpenedAtIndex:index];
      [IntentDonationHelper donateIntent:IntentType::kOpenLatestTab];
      // Check if the item is in current browser.
      // In that case, switch to the tab.
      // Otherwise, open the URL.
      web::WebState* webState = config.localWebState.get();
      WebStateList* webStateList = _browser->GetWebStateList();
      int webStateIndex = WebStateList::kInvalidIndex;
      if (webState) {
        webStateIndex = webStateList->GetIndexOfWebState(webState);
      }
      if (webStateIndex != WebStateList::kInvalidIndex) {
        webStateList->ActivateWebStateAt(webStateIndex);
      } else {
        web::NavigationManager::WebLoadParams webLoadParams =
            web::NavigationManager::WebLoadParams(config.tabURL);
        UrlLoadParams params = UrlLoadParams::SwitchToTab(webLoadParams);
        params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
        _URLLoadingBrowserAgent->Load(params);
      }
      break;
    }
  }
  [self.delegate removeTabResumptionModule];
}

- (void)trackShopCardItem:(TabResumptionConfig*)config {
  __weak TabResumptionMediator* weakSelf = self;

  [PushNotificationUtil requestPushNotificationPermission:^(
                            BOOL granted, BOOL promptShown, NSError* error) {
    web::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](__typeof(self) strongSelf,
               TabResumptionConfig* tabResumptionConfig, BOOL granted,
               BOOL promptShown, NSError* error) {
              if (error) {
                [strongSelf onTracked:ShopCardTrackItemResult::kTrackError
                                 item:tabResumptionConfig];
                return;
              }
              [strongSelf
                  onNotificationPermissionVerifiedOrGranted:tabResumptionConfig
                                                    granted:granted];
            },
            weakSelf, config, granted, promptShown, error));
  }];
  [self.delegate removeTabResumptionModule];
}

- (void)onNotificationPermissionVerifiedOrGranted:(TabResumptionConfig*)config
                                          granted:(BOOL)granted {
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _pushNotificationService->SetPreference(
      identity.gaiaId, PushNotificationClientId::kCommerce, true);

  const bookmarks::BookmarkNode* bookmark =
      _bookmarkModel->GetMostRecentlyAddedUserNodeForURL(config.tabURL);
  bool isNewBookmark = bookmark == nullptr;
  __weak TabResumptionMediator* weakSelf = self;

  auto completionHandler = ^(TabResumptionConfig* tabResumptionConfig,
                             bool success) {
    if (success) {
      [weakSelf
          onTracked:granted
                        ? ShopCardTrackItemResult::kTrackSuccess
                        : ShopCardTrackItemResult::kTrackSuccesNoNotification
               item:tabResumptionConfig];
    } else {
      [weakSelf onTracked:ShopCardTrackItemResult::kTrackError
                     item:tabResumptionConfig];
    }
  };

  if (!bookmark) {
    const bookmarks::BookmarkNode* defaultFolder =
        _bookmarkModel->account_mobile_node();
    if (!defaultFolder) {
      [self onTracked:ShopCardTrackItemResult::kTrackError item:config];
      return;
    }
    bookmark = _bookmarkModel->AddURL(
        defaultFolder, defaultFolder->children().size(),
        base::SysNSStringToUTF16(config.tabTitle), config.tabURL);
  }

  commerce::SetPriceTrackingStateForBookmark(
      _shoppingService, _bookmarkModel, bookmark, true,
      base::BindOnce(completionHandler, config), isNewBookmark,
      config.shopCardData.productInfo);
}

- (void)onTracked:(ShopCardTrackItemResult)result
             item:(TabResumptionConfig*)config {
  [self.dispatcher showSnackbarMessage:[self snackbarMessage:result
                                                        item:config]];
}

- (SnackbarMessage*)snackbarMessage:(ShopCardTrackItemResult)result
                               item:(TabResumptionConfig*)config {
  SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];

  if (result != ShopCardTrackItemResult::kTrackError) {
    // Tracking was successful. Give option to go to price tracking menu.
    action.handler = ^{
      [self.dispatcher showPriceTrackedItems];
    };
  } else {
    // Failed to track - try again.
    action.handler = ^{
      [self trackShopCardItem:config];
    };
  }

  if (result != ShopCardTrackItemResult::kTrackError) {
    action.title = l10n_util::GetNSString(
        IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_SUCCESS_SNACKBAR_ACTION);
    action.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_SUCCESS_SNACKBAR_ACTION);
  } else {
    action.title = l10n_util::GetNSString(
        IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_FAILURE_SNACKBAR_ACTION);
    action.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_FAILURE_SNACKBAR_ACTION);
  }

  SnackbarMessage* message;
  if (result == ShopCardTrackItemResult::kTrackSuccess) {
    message = [[SnackbarMessage alloc]
        initWithTitle:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_SUCCESS_SNACKBAR)];
  } else if (result == ShopCardTrackItemResult::kTrackSuccesNoNotification) {
    message = [[SnackbarMessage alloc]
        initWithTitle:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_NO_PUSH_PERMISSION_SNACKBAR)];
  } else {
    message = [[SnackbarMessage alloc]
        initWithTitle:
            l10n_util::GetNSString(
                IDS_IOS_CONTENT_SUGGESTIONS_SHOPCARD_TRACK_PRICE_FAILURE_SNACKBAR)];
  }

  message.action = action;
  return message;
}

- (void)openDistantTab:(TabResumptionConfig*)config {
  ProfileIOS* profile = _browser->GetProfile();
  sync_sessions::OpenTabsUIDelegate* openTabsDelegate =
      SessionSyncServiceFactory::GetForProfile(profile)
          ->GetOpenTabsUIDelegate();
  const sessions::SessionTab* sessionTab = nullptr;
  if (openTabsDelegate && openTabsDelegate->GetForeignTab(
                              _sessionTag, _tabId.value(), &sessionTab)) {
    bool isNTP = _webStateList->GetActiveWebState()->GetVisibleURL() ==
                 kChromeUINewTabURL;
    new_tab_page_uma::RecordNTPAction(
        _isOffTheRecord, isNTP,
        new_tab_page_uma::ACTION_OPENED_FOREIGN_SESSION);

    std::unique_ptr<web::WebState> webState =
        session_util::CreateWebStateWithNavigationEntries(
            profile, sessionTab->current_navigation_index,
            sessionTab->navigations);
    _webStateList->ReplaceWebStateAt(_webStateList->active_index(),
                                     std::move(webState));
  } else {
    web::NavigationManager::WebLoadParams webLoadParams =
        web::NavigationManager::WebLoadParams(config.tabURL);
    UrlLoadParams params = UrlLoadParams::InCurrentTab(webLoadParams);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    _URLLoadingBrowserAgent->Load(params);
  }
}

- (void)disableModule {
  tab_resumption_prefs::DisableTabResumption(_profilePrefs);
}

- (void)setDelegate:(id<TabResumptionMediatorDelegate>)delegate {
  _delegate = delegate;
  if (_delegate) {
    [self fetchLastTabResumptionItem];
  }
}

#pragma mark - MagicStackModuleDelegate

- (void)magicStackModule:(MagicStackModule*)magicStackModule
     wasDisplayedAtIndex:(NSUInteger)index {
  CHECK(self.itemConfig == magicStackModule);
  _currentlyTopModule = (index == 0);
  switch (self.itemConfig.itemType) {
    case TabResumptionItemType::kLastSyncedTab:
      [self.NTPActionsDelegate distantTabResumptionDisplayedAtIndex:index];
      break;
    case TabResumptionItemType::kMostRecentTab:
      [self.NTPActionsDelegate recentTabTileDisplayedAtIndex:index];
      break;
  }
  [self.contentSuggestionsMetricsRecorder
      recordTabResumptionImpressionWithCustomization:
          static_cast<TabResumptionConfig*>(magicStackModule).shopCardData
                                             atIndex:index];

  if (IsShopCardImpressionLimitsEnabled() && index == 0 &&
      _impressionLimitService) {
    _impressionLimitService->LogImpressionForURL(self.itemConfig.tabURL,
                                                 GetImpressionLimitPref());
  }
}

#pragma mark - Boolean Observer

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _tabResumptionDisabled) {
    if (!observableBoolean.value) {
      [self.delegate removeTabResumptionModule];
    }
  }
}

#pragma mark - SyncObserverBridge

- (void)onSyncStateChanged {
  // If tabs are not synced, hide the tab resumption tile.
  if (!_syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kTabs)) {
    [self.delegate removeTabResumptionModule];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      // If the user is signed out, remove the tab resumption tile.
      [self.delegate removeTabResumptionModule];
      self.itemConfig = nil;
      break;
    }
    default:
      break;
  }
}

#pragma mark - SyncedSessionsObserver

- (void)onForeignSessionsChanged {
  [self fetchLastTabResumptionItem];
}

#pragma mark - StartSurfaceRecentTabObserving

- (void)mostRecentTabWasRemoved:(web::WebState*)webState {
  if (self.itemConfig && self.itemConfig.itemType == kMostRecentTab) {
    [self.delegate removeTabResumptionModule];
    _currentlyTopModule = NO;
    self.itemConfig = nil;
  }
}

- (void)mostRecentTab:(web::WebState*)webState
    faviconUpdatedWithImage:(UIImage*)image {
}

- (void)mostRecentTab:(web::WebState*)webState
      titleWasUpdated:(NSString*)title {
}

#pragma mark - Private

// Updates the Tab Resumption card view for the configuration of a given
// `config`.
- (void)updateCardWithConfig:(TabResumptionConfig*)config {
  [self.itemConfig reconfigureWithConfig:config];
  [self.delegate tabResumptionMediatorDidReconfigureItem];
}

// Fetches the item to display from the model.
- (void)fetchLastTabResumptionItem {
  if (tab_resumption_prefs::IsTabResumptionDisabled(_profilePrefs)) {
    return;
  }

  _sessionTag = "";
  _tabId = SessionID::InvalidValue();

  std::optional<base::Time> mostRecentTabOpenedTime;
  std::optional<base::Time> lastSyncedTabSyncedTime;

  web::WebState* mostRecentTab = _recentTabBrowserAgent->most_recent_tab();
  if (mostRecentTab) {
    mostRecentTabOpenedTime =
        GetTimeMostRecentTabWasOpenForSceneState(_sceneState);
  }

  const synced_sessions::DistantSession* session = nullptr;
  const synced_sessions::DistantTab* tab = nullptr;
  const synced_sessions::SyncedSessions syncedSessions(_sessionSyncService);
  LastActiveDistantTab lastDistantTab = GetLastActiveDistantTab(
      syncedSessions, TabResumptionForXDevicesTimeThreshold());
  if (lastDistantTab.tab) {
    tab = lastDistantTab.tab;
    if (_lastDistinctItemURL != tab->virtual_url) {
      _lastDistinctItemURL = tab->virtual_url;
      session = lastDistantTab.session;
      lastSyncedTabSyncedTime = tab->last_active_time;
    }
  }

  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  bool canShowMostRecentItem =
      activeWebState && NewTabPageTabHelper::FromWebState(activeWebState)
                            ->ShouldShowStartSurface();
  // If both times have not been updated, that means there is no item to return.
  if (!mostRecentTabOpenedTime.has_value() &&
      !lastSyncedTabSyncedTime.has_value()) {
    return;
  }

  if (lastSyncedTabSyncedTime > mostRecentTabOpenedTime &&
      !base::FeatureList::IsEnabled(kIOSRemoteTabResumptionKillSwitch)) {
    [self fetchLastSyncedTabItemFromLastActiveDistantTab:tab session:session];
    _sessionTag = session->tag;
    _tabId = tab->tab_id;
  } else if (canShowMostRecentItem && mostRecentTab) {
    [self fetchMostRecentTabItemFromWebState:mostRecentTab
                                  openedTime:mostRecentTabOpenedTime.value_or(
                                                 base::Time::UnixEpoch())];
  }
}

- (void)fetchShopCardDataForItemIfApplicable:(TabResumptionConfig*)item
                                         url:(const GURL&)resumptionURL {
  if (IsShopCardImpressionLimitsEnabled() && _impressionLimitService) {
    // TODO(crbug.com/408252386) Add unit tests for impression count
    // integration.
    std::optional<int> count = _impressionLimitService->GetImpressionCount(
        resumptionURL, GetImpressionLimitPref());
    if (count.has_value() && count.value() >= GetImpressionLimit()) {
      return;
    }
  }

  if (commerce::kShopCardVariation.Get().contains(commerce::kShopCardArm3) ||
      commerce::kShopCardVariation.Get() == commerce::kShopCardArm4) {
    GURL url = resumptionURL;
    __weak __typeof(self) weakSelf = self;
    _shoppingService->GetAllPriceTrackedBookmarks(base::BindOnce(
        ^(std::vector<const bookmarks::BookmarkNode*> subscriptions) {
          TabResumptionMediator* strongSelf = weakSelf;
          if (!strongSelf || !strongSelf.delegate) {
            return;
          }
          if (subscriptions.empty()) {
            [strongSelf fetchImageForItem:item];
          } else {
            [strongSelf onPriceTrackedBookmarksReceived:subscriptions
                                                    url:url
                                                   item:item];
          }
        }));
  } else {
    // Fetch the favicon.
    [self fetchImageForItem:item];
  }
}

- (void)onPriceTrackedBookmarksReceived:
            (std::vector<const bookmarks::BookmarkNode*>)subscriptions
                                    url:(const GURL&)resumptionUrl
                                   item:(TabResumptionConfig*)config {
  if (!resumptionUrl.is_valid()) {
    return;
  }
  // Remove module if already tracking the product.
  if (std::ranges::any_of(subscriptions, [&](const auto& bookmark) {
        return bookmark->url() == resumptionUrl;
      })) {
    [self.delegate removeTabResumptionModule];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  TabResumptionMediatorProxy::CanApplyOptimizationOnDemand(
      _optimizationGuideService, resumptionUrl,
      optimization_guide::proto::PRICE_TRACKING,
      optimization_guide::proto::RequestContext::CONTEXT_SHOP_CARD,
      base::BindRepeating(
          ^(const GURL& url,
            const base::flat_map<
                optimization_guide::proto::OptimizationType,
                optimization_guide::OptimizationGuideDecisionWithMetadata>&
                decisions) {
            ConfigureTabResumptionItemForShopCard(decisions, config, url);
            // Fetch the favicon.
            web::GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE, base::BindOnce(^{
                  [weakSelf fetchImageForItem:config];
                }));
          }));
}

// Fetches a relevant image for the `config` to display.
- (void)fetchImageForItem:(TabResumptionConfig*)config {
  if ([self isPendingItem:config]) {
    // The item was already fetched or is being fetched, ignore it.
    return;
  }
  _pendingItem = config;
  if (ShouldShowItemImmediately()) {
    [self showItem:config];
  }
  if (config.shopCardData.productImageURL.has_value()) {
    [self
        salientImageURLReceived:GURL(
                                    config.shopCardData.productImageURL.value())
                        forItem:config
                    updateImage:NO];
  } else {
    if (config.itemType == kMostRecentTab) {
      [self fetchSnapshotForItem:config];
    }
  }
  [self fetchFaviconForItem:config];
}

// Arm 6 delays acquiring the price drop (if it exists) and the
// product image and updates the card when this data is availalbe.
// This reduces the overall latency of the card.
- (void)fetchPriceDropIfApplicable:(TabResumptionConfig*)config {
  if (commerce::kShopCardVariation.Get() != commerce::kShopCardArm6) {
    return;
  }
  __weak TabResumptionMediator* weakSelf = self;
  web::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                             [weakSelf fetchPriceDrop:config];
                                           }));
}

- (void)fetchPriceDrop:(TabResumptionConfig*)config {
  __weak __typeof(self) weakSelf = self;
  TabResumptionMediatorProxy::CanApplyOptimizationOnDemand(
      _optimizationGuideService, config.tabURL,
      optimization_guide::proto::PRICE_TRACKING,
      optimization_guide::proto::RequestContext::CONTEXT_SHOP_CARD,
      base::BindRepeating(^(
          const GURL& url,
          const base::flat_map<
              optimization_guide::proto::OptimizationType,
              optimization_guide::OptimizationGuideDecisionWithMetadata>&
              decisions) {
        TabResumptionMediator* strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }

        ConfigureTabResumptionItemForShopCard(decisions, config, url);
        if (![strongSelf isPendingItem:config]) {
          // The item was already fetched or is being fetched, ignore it.
          return;
        }

        web::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(^{
              if (config.shopCardData.productImageURL.has_value()) {
                [strongSelf
                    salientImageURLReceived:GURL(config.shopCardData
                                                     .productImageURL.value())
                                    forItem:config
                                updateImage:YES];
              } else {
                [self updateCardWithConfig:config];
              }
            }));
      }));
}

// Fetches the snapshot of the tab showing `config`.
- (void)fetchSnapshotForItem:(TabResumptionConfig*)config {
  if (!config.localWebState) {
    return;
  }

  web::WebState* webState = config.localWebState.get();
  BrowserList* browserList =
      BrowserListFactory::GetForProfile(_browser->GetProfile());
  Browser* webStateBrowser = GetBrowserForTabWithCriteria(
      browserList,
      WebStateSearchCriteria{.identifier = webState->GetUniqueIdentifier()},
      false);

  if (webStateBrowser) {
    __weak __typeof(self) weakSelf = self;
    SnapshotBrowserAgent::FromBrowser(webStateBrowser)
        ->RetrieveSnapshotWithID(SnapshotID(webState->GetUniqueIdentifier()),
                                 SnapshotKindColor, ^(UIImage* image) {
                                   [weakSelf snapshotFetched:image
                                                     forItem:config];
                                 });
  }
}

// The snapshot of the tab showing `config` was fetched.
- (void)snapshotFetched:(UIImage*)image forItem:(TabResumptionConfig*)config {
  if (!image) {
    return;
  }
  config.contentImage = image;
  [self showItem:config];
}

// The URL for the salient image has been received. Download the image if it
// is valid or fallbacks to favicon.
- (void)salientImageURLReceived:(const GURL&)URL
                        forItem:(TabResumptionConfig*)config
                    updateImage:(BOOL)updateImage {
  __weak TabResumptionMediator* weakSelf = self;
  if (!URL.is_valid() || !URL.SchemeIsCryptographic() ||
      !base::EndsWith(URL.GetHost(), kGStatic)) {
    return;
  }
  _imageFetcher->FetchImageData(
      URL,
      base::BindOnce(^(const std::string& imageData,
                       const image_fetcher::RequestMetadata& metadata) {
        [weakSelf salientImageReceived:imageData
                               forItem:config
                           updateImage:updateImage];
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// Salient image has been received. Display it.
- (void)salientImageReceived:(const std::string&)imageData
                     forItem:(TabResumptionConfig*)config
                 updateImage:(BOOL)updateImage {
  UIImage* image =
      [UIImage imageWithData:[NSData dataWithBytes:imageData.c_str()
                                            length:imageData.size()]];
  if (!image) {
    return;
  }
  config.contentImage = image;
  if (updateImage) {
    [self updateCardWithConfig:config];
  } else {
    [self showItem:config];
  }
}

// Fetches the favicon for `config`.
- (void)fetchFaviconForItem:(TabResumptionConfig*)config {
  __weak TabResumptionMediator* weakSelf = self;
  if (!_faviconLoader) {
    return;
  }
  _faviconLoader->FaviconForPageUrl(
      config.tabURL, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true,
      ^(FaviconAttributes* attributes, bool cached) {
        [weakSelf faviconReceived:attributes cached:cached forItem:config];
      });
}

// The favicon has been received. Display it.
- (void)faviconReceived:(FaviconAttributes*)attributes
                 cached:(BOOL)cached
                forItem:(TabResumptionConfig*)config {
  if (config.faviconImage || !cached) {
    if ([UIImagePNGRepresentation(config.faviconImage)
            isEqual:UIImagePNGRepresentation(attributes.faviconImage)]) {
      return;
    }
    config.faviconImage = attributes.faviconImage;
    [self showItem:config];
  }
}

// Sends `config` to  TabResumption to be displayed.
- (void)showItem:(TabResumptionConfig*)config {
  if (![self isPendingItem:config]) {
    // A new item has been fetched, ignore.
    return;
  }
  if (!self.itemConfig) {
    self.itemConfig = config;
    [self.delegate tabResumptionMediatorDidReceiveItem];
    [self fetchPriceDropIfApplicable:config];
    return;
  }

  // The item is already used by some view, so it cannot be replaced.
  // Instead the existing config must be updated.
  [self updateCardWithConfig:config];
  [self fetchPriceDropIfApplicable:config];
}

// Creates a TabResumptionConfig corresponding to the last synced tab.
- (void)fetchLastSyncedTabItemFromLastActiveDistantTab:
            (const synced_sessions::DistantTab*)tab
                                               session:(const synced_sessions::
                                                            DistantSession*)
                                                           session {
  TabResumptionConfig* config = [[TabResumptionConfig alloc]
      initWithItemType:TabResumptionItemType::kLastSyncedTab];
  config.sessionName = base::SysUTF8ToNSString(session->name);
  config.tabTitle = base::SysUTF16ToNSString(tab->title);
  config.syncedTime = tab->last_active_time;
  config.tabURL = tab->virtual_url;
  config.commandHandler = self;
  config.delegate = self;
  config.shouldShowSeeMore = YES;
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm4) {
    config.shouldShowSeeMore = NO;
  }
  [self fetchShopCardDataForItemIfApplicable:config url:tab->virtual_url];
}

// Creates a TabResumptionConfig corresponding to the `webState`.
- (void)fetchMostRecentTabItemFromWebState:(web::WebState*)webState
                                openedTime:(base::Time)openedTime {
  TabResumptionConfig* config = [[TabResumptionConfig alloc]
      initWithItemType:TabResumptionItemType::kMostRecentTab];
  config.tabTitle = base::SysUTF16ToNSString(webState->GetTitle());
  config.syncedTime = openedTime;
  config.tabURL = webState->GetLastCommittedURL();
  config.localWebState = webState->GetWeakPtr();
  config.commandHandler = self;
  config.delegate = self;
  config.shouldShowSeeMore = YES;
  if (commerce::kShopCardVariation.Get() == commerce::kShopCardArm4) {
    config.shouldShowSeeMore = NO;
  }
  [self fetchShopCardDataForItemIfApplicable:config
                                         url:webState->GetLastCommittedURL()];
}

// Compares `config` and `_pendingItem` on tabURL and tabTitle field.
- (BOOL)isPendingItem:(TabResumptionConfig*)config {
  if (_pendingItem == nil) {
    return NO;
  }
  return config.tabURL == _pendingItem.tabURL &&
         [config.tabTitle isEqualToString:_pendingItem.tabTitle];
}

@end
