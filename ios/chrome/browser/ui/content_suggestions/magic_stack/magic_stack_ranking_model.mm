// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"

#import <optional>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/shopping_service.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/segmentation_platform/embedder/home_modules/constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module.h"
#import "components/segmentation_platform/embedder/home_modules/tips_ephemeral_module_constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/ntp/ui_bundled/home_start_data_source.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_item.h"
#import "ios/chrome/browser/ui/content_suggestions/price_tracking_promo/price_tracking_promo_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_config.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/utils.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_helper_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_item.h"
#import "ios/chrome/browser/ui/content_suggestions/tab_resumption/tab_resumption_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_magic_stack_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/tips/tips_module_state.h"

using segmentation_platform::TipIdentifier;
using segmentation_platform::home_modules::TipIdentifierForOutputLabel;
using segmentation_platform::home_modules::TipsEphemeralModule;

@interface MagicStackRankingModel () <MostVisitedTilesMediatorDelegate,
                                      ParcelTrackingMediatorDelegate,
                                      PriceTrackingPromoMediatorDelegate,
                                      SafetyCheckMagicStackMediatorDelegate,
                                      SetUpListMediatorAudience,
                                      ShortcutsMediatorDelegate,
                                      TabResumptionHelperDelegate>
// For testing-only
@property(nonatomic, assign) BOOL hasReceivedMagicStackResponse;
@property(nonatomic, assign) BOOL hasReceivedEphemericalCardResponse;
@end

@implementation MagicStackRankingModel {
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      _segmentationService;
  raw_ptr<commerce::ShoppingService> _shoppingService;
  raw_ptr<AuthenticationService> _authService;
  raw_ptr<PrefService> _prefService;
  raw_ptr<PrefService> _localState;
  // The latest module ranking returned from the SegmentationService.
  NSArray<NSNumber*>* _magicStackOrderFromSegmentation;
  // YES if the module ranking has been received from the SegmentationService.
  BOOL _magicStackOrderFromSegmentationReceived;
  // The latest Magic Stack module order sent up to the consumer. This includes
  // any omissions due to filtering from `_magicStackOrderFromSegmentation` and
  // any additions beyond `_magicStackOrderFromSegmentation` (e.g. Set Up List).
  NSArray<MagicStackModule*>* _latestMagicStackConfigOrder;
  // Module mediators.
  MostVisitedTilesMediator* _mostVisitedTilesMediator;
  SetUpListMediator* _setUpListMediator;
  TabResumptionMediator* _tabResumptionMediator;
  ParcelTrackingMediator* _parcelTrackingMediator;
  PriceTrackingPromoMediator* _priceTrackingPromoMediator;
  ShortcutsMediator* _shortcutsMediator;
  SafetyCheckMagicStackMediator* _safetyCheckMediator;
  TipsMagicStackMediator* _tipsMediator;
  raw_ptr<TipsManagerIOS> _tipsManager;
  base::TimeTicks ranking_fetch_start_time_;
  ContentSuggestionsModuleType _ephemeralCardToShow;
}

- (instancetype)
    initWithSegmentationService:
        (segmentation_platform::SegmentationPlatformService*)segmentationService
                shoppingService:(commerce::ShoppingService*)shoppingService
                    authService:(AuthenticationService*)authenticationService
                    prefService:(PrefService*)prefService
                     localState:(PrefService*)localState
                moduleMediators:(NSArray*)moduleMediators
                    tipsManager:(TipsManagerIOS*)tipsManager {
  self = [super init];
  if (self) {
    _segmentationService = segmentationService;
    _shoppingService = shoppingService;
    _authService = authenticationService;
    _prefService = prefService;
    _localState = localState;
    _ephemeralCardToShow = ContentSuggestionsModuleType::kInvalid;

    if (IsTipsMagicStackEnabled()) {
      CHECK(tipsManager);
      _tipsManager = tipsManager;
    }

    for (id mediator in moduleMediators) {
      if ([mediator isKindOfClass:[MostVisitedTilesMediator class]]) {
        _mostVisitedTilesMediator =
            static_cast<MostVisitedTilesMediator*>(mediator);
        _mostVisitedTilesMediator.delegate = self;
      } else if ([mediator isKindOfClass:[SetUpListMediator class]]) {
        _setUpListMediator = static_cast<SetUpListMediator*>(mediator);
        _setUpListMediator.audience = self;
      } else if ([mediator isKindOfClass:[TabResumptionMediator class]]) {
        _tabResumptionMediator = static_cast<TabResumptionMediator*>(mediator);
        _tabResumptionMediator.delegate = self;
      } else if ([mediator isKindOfClass:[ShortcutsMediator class]]) {
        _shortcutsMediator = static_cast<ShortcutsMediator*>(mediator);
        _shortcutsMediator.delegate = self;
      } else if ([mediator isKindOfClass:[ParcelTrackingMediator class]]) {
        _parcelTrackingMediator =
            static_cast<ParcelTrackingMediator*>(mediator);
        _parcelTrackingMediator.delegate = self;
      } else if ([mediator isKindOfClass:[PriceTrackingPromoMediator class]]) {
        _priceTrackingPromoMediator =
            static_cast<PriceTrackingPromoMediator*>(mediator);
        _priceTrackingPromoMediator.delegate = self;
      } else if ([mediator
                     isKindOfClass:[SafetyCheckMagicStackMediator class]]) {
        _safetyCheckMediator =
            static_cast<SafetyCheckMagicStackMediator*>(mediator);
        _safetyCheckMediator.delegate = self;
      } else if ([mediator isKindOfClass:[TipsMagicStackMediator class]]) {
        _tipsMediator = static_cast<TipsMagicStackMediator*>(mediator);
      } else {
        // Known module mediators need to be handled.
        NOTREACHED_IN_MIGRATION();
      }
    }
  }
  return self;
}

- (void)disconnect {
  _mostVisitedTilesMediator = nil;
  _setUpListMediator = nil;
  _tabResumptionMediator = nil;
  _parcelTrackingMediator = nil;
  _priceTrackingPromoMediator = nil;
  _shortcutsMediator = nil;
  _safetyCheckMediator = nil;
  _tipsMediator = nil;
  _tipsManager = nil;
}

#pragma mark - Public

- (void)fetchLatestMagicStackRanking {
  _magicStackOrderFromSegmentationReceived = NO;
  _magicStackOrderFromSegmentation = nil;
  _latestMagicStackConfigOrder = nil;
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformEphemeralCardRanker)) {
    _ephemeralCardToShow = ContentSuggestionsModuleType::kInvalid;
    [self fetchEphemeralCardFromSegmentationPlatform];
  }
  [self fetchMagicStackModuleRankingFromSegmentationPlatform];
}

- (void)logMagicStackEngagementForType:(ContentSuggestionsModuleType)type {
  [self.contentSuggestionsMetricsRecorder
      recordMagicStackModuleEngagementForType:type
                                      atIndex:
                                          [self indexForMagicStackModule:type]];
}

#pragma mark - SetUpListMediatorAudience

- (void)removeSetUpList {
  base::UmaHistogramEnumeration(
      kMagicStackModuleDisabledHistogram,
      ContentSuggestionsModuleType::kCompactedSetUpList);
  [self.delegate magicStackRankingModel:self
                          didRemoveItem:_setUpListMediator.setUpListConfigs[0]];
}

- (void)replaceSetUpListWithAllSet:(SetUpListConfig*)allSetConfig {
  [self.delegate magicStackRankingModel:self
                         didReplaceItem:_setUpListMediator.setUpListConfigs[0]
                               withItem:allSetConfig];
}

#pragma mark - SafetyCheckMagicStackMediatorDelegate

- (void)removeSafetyCheckModule {
  if (![self isMagicStackOrderReady]) {
    return;
  }

  base::UmaHistogramEnumeration(kMagicStackModuleDisabledHistogram,
                                ContentSuggestionsModuleType::kSafetyCheck);
  [self.delegate magicStackRankingModel:self
                          didRemoveItem:_safetyCheckMediator.safetyCheckState];
}

#pragma mark - TabResumptionHelperDelegate

- (void)tabResumptionHelperDidReceiveItem {
  CHECK(IsTabResumptionEnabled());
  if (tab_resumption_prefs::IsTabResumptionDisabled(
          IsHomeCustomizationEnabled() ? _prefService : _localState)) {
    return;
  }

  [self showTabResumptionWithItem:_tabResumptionMediator.itemConfig];
}

- (void)tabResumptionHelperDidReconfigureItem {
  if (tab_resumption_prefs::IsTabResumptionDisabled(
          IsHomeCustomizationEnabled() ? _prefService : _localState)) {
    return;
  }
  TabResumptionItem* item = _tabResumptionMediator.itemConfig;
  [self.delegate magicStackRankingModel:self didReconfigureItem:item];
}

- (void)removeTabResumptionModule {
  [self.delegate magicStackRankingModel:self
                          didRemoveItem:_tabResumptionMediator.itemConfig];
}

#pragma mark - ParcelTrackingMediatorDelegate

- (void)newParcelsAvailable {
  MagicStackModule* item = _parcelTrackingMediator.parcelTrackingItemToShow;
  NSArray<MagicStackModule*>* rank = [self latestMagicStackConfigRank];
  NSUInteger index = [rank indexOfObject:item];
  if (index == NSNotFound) {
    return;
  }
  [self.delegate magicStackRankingModel:self didInsertItem:item atIndex:index];
}

- (void)parcelTrackingDisabled {
  base::UmaHistogramEnumeration(kMagicStackModuleDisabledHistogram,
                                ContentSuggestionsModuleType::kParcelTracking);
  [self.delegate
      magicStackRankingModel:self
               didRemoveItem:_parcelTrackingMediator.parcelTrackingItemToShow];
}

- (NSUInteger)indexForMagicStackModule:
    (ContentSuggestionsModuleType)moduleType {
  return [_latestMagicStackConfigOrder
      indexOfObjectPassingTest:^BOOL(MagicStackModule* config, NSUInteger idx,
                                     BOOL* stop) {
        return config.type == moduleType;
      }];
}

#pragma mark - MostVisitedTilesMediatorDelegate

- (void)didReceiveInitialMostVistedTiles {
  if (![self isMagicStackOrderReady]) {
    return;
  }

  NSArray<MagicStackModule*>* rank = [self latestMagicStackConfigRank];
  NSUInteger index =
      [rank indexOfObject:_mostVisitedTilesMediator.mostVisitedConfig];
  [self.delegate
      magicStackRankingModel:self
               didInsertItem:_mostVisitedTilesMediator.mostVisitedConfig
                     atIndex:index];
}

- (void)removeMostVisitedTilesModule {
  if (![self isMagicStackOrderReady]) {
    return;
  }

  [self.delegate
      magicStackRankingModel:self
               didRemoveItem:_mostVisitedTilesMediator.mostVisitedConfig];
}

#pragma mark - Private

// Adds the correct Set Up List module type to the Magic Stack `order`.
- (void)addSetUpListToMagicStackOrder:(NSMutableArray*)order {
  if ([_setUpListMediator allItemsComplete]) {
    [order addObject:@(int(ContentSuggestionsModuleType::kSetUpListAllSet))];
  } else if (set_up_list_utils::ShouldShowCompactedSetUpListModule()) {
    [order addObject:@(int(ContentSuggestionsModuleType::kCompactedSetUpList))];
  } else {
    for (SetUpListItemViewData* model in _setUpListMediator.setUpListItems) {
      [order addObject:@(int(SetUpListModuleTypeForSetUpListType(model.type)))];
    }
  }
}

// Adds the Safety Check module to `order` based on the current Safety Check
// state.
- (void)addSafetyCheckToMagicStackOrder:(NSMutableArray*)order {
  CHECK(IsSafetyCheckMagicStackEnabled());
  [order addObject:@(int(ContentSuggestionsModuleType::kSafetyCheck))];
}

// New subscription observed for user (from another platform). This
// has the potential to boost the ranking of the price trackiing promo.
- (void)newSubscriptionAvailable {
  MagicStackModule* item =
      _priceTrackingPromoMediator.priceTrackingPromoItemToShow;
  NSArray<MagicStackModule*>* rank = [self latestMagicStackConfigRank];
  NSUInteger index = [rank indexOfObject:item];
  if (index == NSNotFound) {
    return;
  }
  [self.delegate magicStackRankingModel:self didInsertItem:item atIndex:index];
}

// Starts a fetch of the ephemeral card to show from Segmentation.
- (void)fetchEphemeralCardFromSegmentationPlatform {
  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = true;
  auto inputContext =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  // This check has to match check in HomeModulesCardRegistry::CreateAllCards()
  // so that expected inputs match passed inputs.
  if (base::FeatureList::IsEnabled(commerce::kPriceTrackingPromo)) {
    inputContext->metadata_args.emplace(
        segmentation_platform::kIsNewUser,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            IsFirstRunRecent(base::Days(14))));
    inputContext->metadata_args.emplace(
        segmentation_platform::kIsSynced,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _shoppingService->IsShoppingListEligible()));
  }

  if (IsTipsMagicStackEnabled() && _tipsManager) {
    // Profile signals
    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::kLensUsed,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(
                segmentation_platform::tips_manager::signals::kLensUsed)));

    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::kOpenedShoppingWebsite,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(segmentation_platform::tips_manager::
                                             signals::kOpenedShoppingWebsite)));

    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::
            kOpenedWebsiteInAnotherLanguage,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(
                segmentation_platform::tips_manager::signals::
                    kOpenedWebsiteInAnotherLanguage)));

    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::kSavedPasswords,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(segmentation_platform::tips_manager::
                                             signals::kSavedPasswords)));

    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::kUsedGoogleTranslation,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(segmentation_platform::tips_manager::
                                             signals::kUsedGoogleTranslation)));

    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::kUsedPasswordAutofill,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(segmentation_platform::tips_manager::
                                             signals::kUsedPasswordAutofill)));

    inputContext->metadata_args.emplace(
        segmentation_platform::kHasEnhancedSafeBrowsing,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _prefService->GetBoolean(prefs::kSafeBrowsingEnhanced)));

    // Local signals
    inputContext->metadata_args.emplace(
        segmentation_platform::tips_manager::signals::
            kAddressBarPositionChoiceScreenDisplayed,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            _tipsManager->WasSignalFired(
                segmentation_platform::tips_manager::signals::
                    kAddressBarPositionChoiceScreenDisplayed)));
  }

  __weak MagicStackRankingModel* weakSelf = self;
  _segmentationService->GetClassificationResult(
      segmentation_platform::kEphemeralHomeModuleBackendKey, options,
      inputContext,
      base::BindOnce(
          ^(const segmentation_platform::ClassificationResult& result) {
            weakSelf.hasReceivedEphemericalCardResponse = YES;
            [weakSelf didReceiveEphemeralCardSegmentationResult:result];
          }));
}

// Handles the ephemeral card Segmentation response and adds a card if there is
// one to show.
- (void)didReceiveEphemeralCardSegmentationResult:
    (const segmentation_platform::ClassificationResult&)result {
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return;
  }

  MagicStackModule* card;

  for (const std::string& label : result.ordered_labels) {
    if (label == segmentation_platform::kPriceTrackingNotificationPromo) {
      if (IsPriceTrackingPromoCardEnabled(_shoppingService, _authService,
                                          _prefService)) {
        _ephemeralCardToShow =
            ContentSuggestionsModuleType::kPriceTrackingPromo;
        card = _priceTrackingPromoMediator.priceTrackingPromoItemToShow;
        break;
      }
    } else if (TipsEphemeralModule::IsModuleLabel(label) &&
               IsTipsMagicStackEnabled()) {
      TipIdentifier tipIdentifier = TipIdentifierForOutputLabel(label);

      if (tipIdentifier != TipIdentifier::kUnknown) {
        _ephemeralCardToShow =
            (tipIdentifier == TipIdentifier::kLensShop &&
             TipsLensShopExperimentTypeEnabled() ==
                 TipsLensShopExperimentType::kWithProductImage)
                ? ContentSuggestionsModuleType::kTipsWithProductImage
                : ContentSuggestionsModuleType::kTips;

        [_tipsMediator reconfigureWithTipIdentifier:tipIdentifier];

        card = _tipsMediator.state;

        break;
      }
    }
  }
  if (_ephemeralCardToShow != ContentSuggestionsModuleType::kInvalid && card) {
    [self addEphemeralCardToMagicStack:card];
  }
}

// Re-calculates the Magic Stack order and inserts the new ephemeral `card` if
// the Magic Stack ranking has been received.
- (void)addEphemeralCardToMagicStack:(MagicStackModule*)card {
  if (!_magicStackOrderFromSegmentationReceived) {
    return;
  }

  _latestMagicStackConfigOrder = [self latestMagicStackConfigRank];
  [self.delegate magicStackRankingModel:self didInsertItem:card atIndex:0];
}

- (void)removePriceTrackingPromo {
  [self.delegate magicStackRankingModel:self
                          didRemoveItem:_priceTrackingPromoMediator
                                            .priceTrackingPromoItemToShow];
}

// Starts a fetch of the Segmentation module ranking.
- (void)fetchMagicStackModuleRankingFromSegmentationPlatform {
  if (!base::FeatureList::IsEnabled(segmentation_platform::features::
                                        kSegmentationPlatformIosModuleRanker)) {
    segmentation_platform::ClassificationResult result(
        segmentation_platform::PredictionStatus::kNotReady);
    self.hasReceivedMagicStackResponse = YES;
    [self didReceiveSegmentationServiceResult:result];
    return;
  }
  auto inputContext =
      base::MakeRefCounted<segmentation_platform::InputContext>();
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformIosModuleRankerSplitBySurface)) {
    inputContext->metadata_args.emplace(
        segmentation_platform::kIsShowingStartSurface,
        segmentation_platform::processing::ProcessedValue::FromFloat(
            [self.homeStartDataSource isStartSurface]));
  }
  int mvtFreshnessImpressionCount = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationMVTImpressionsSinceFreshness);
  inputContext->metadata_args.emplace(
      segmentation_platform::kMostVisitedTilesFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          mvtFreshnessImpressionCount));
  int shortcutsFreshnessImpressionCount = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationShortcutsImpressionsSinceFreshness);
  inputContext->metadata_args.emplace(
      segmentation_platform::kShortcutsFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          shortcutsFreshnessImpressionCount));
  int safetyCheckFreshnessImpressionCount = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationSafetyCheckImpressionsSinceFreshness);
  inputContext->metadata_args.emplace(
      segmentation_platform::kSafetyCheckFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          safetyCheckFreshnessImpressionCount));
  int tabResumptionFreshnessImpressionCount = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationTabResumptionImpressionsSinceFreshness);
  inputContext->metadata_args.emplace(
      segmentation_platform::kTabResumptionFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          tabResumptionFreshnessImpressionCount));
  int parcelTrackingFreshnessImpressionCount = _localState->GetInteger(
      prefs::kIosMagicStackSegmentationParcelTrackingImpressionsSinceFreshness);
  inputContext->metadata_args.emplace(
      segmentation_platform::kParcelTrackingFreshness,
      segmentation_platform::processing::ProcessedValue::FromFloat(
          parcelTrackingFreshnessImpressionCount));
  __weak MagicStackRankingModel* weakSelf = self;
  segmentation_platform::PredictionOptions options;

  if (base::FeatureList::IsEnabled(
          kSegmentationPlatformIosModuleRankerCaching)) {
    // Ignores tab resumption freshness since local tab always logs a freshness
    // signal for Start.
    BOOL hasNoFreshnessSignal = shortcutsFreshnessImpressionCount != 0 &&
                                parcelTrackingFreshnessImpressionCount != 0;
    if (IsSafetyCheckMagicStackEnabled()) {
      hasNoFreshnessSignal =
          hasNoFreshnessSignal && safetyCheckFreshnessImpressionCount != 0;
    }
    if (hasNoFreshnessSignal && [self.homeStartDataSource isStartSurface]) {
      options = segmentation_platform::PredictionOptions::ForCached(true);
    } else {
      options = segmentation_platform::PredictionOptions::ForOnDemand(true);
    }
    options.can_update_cache_for_future_requests = true;
  } else {
    options.on_demand_execution = true;
  }
  ranking_fetch_start_time_ = base::TimeTicks::Now();
  _segmentationService->GetClassificationResult(
      segmentation_platform::kIosModuleRankerKey, options, inputContext,
      base::BindOnce(
          ^(const segmentation_platform::ClassificationResult& result) {
            weakSelf.hasReceivedMagicStackResponse = YES;
            [weakSelf didReceiveSegmentationServiceResult:result];
          }));
}

- (void)didReceiveSegmentationServiceResult:
    (const segmentation_platform::ClassificationResult&)result {
  if (result.status != segmentation_platform::PredictionStatus::kSucceeded) {
    return;
  }

  if ([self.homeStartDataSource isStartSurface]) {
    base::UmaHistogramMediumTimes(
        kMagicStackStartSegmentationRankingFetchTimeHistogram,
        base::TimeTicks::Now() - ranking_fetch_start_time_);
  } else {
    base::UmaHistogramMediumTimes(
        kMagicStackNTPSegmentationRankingFetchTimeHistogram,
        base::TimeTicks::Now() - ranking_fetch_start_time_);
  }

  NSMutableArray* magicStackOrder = [NSMutableArray array];
  for (const std::string& label : result.ordered_labels) {
    if (label == segmentation_platform::kMostVisitedTiles) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kMostVisited))];
    } else if (label == segmentation_platform::kShortcuts) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kShortcuts))];
    } else if (label == segmentation_platform::kSafetyCheck) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kSafetyCheck))];
    } else if (label == segmentation_platform::kTabResumption) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kTabResumption))];
    } else if (label == segmentation_platform::kParcelTracking) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kParcelTracking))];
    } else if (label == segmentation_platform::kPriceTrackingPromo) {
      [magicStackOrder
          addObject:@(int(ContentSuggestionsModuleType::kPriceTrackingPromo))];
    }
  }
  _magicStackOrderFromSegmentationReceived = YES;
  _magicStackOrderFromSegmentation = magicStackOrder;
  _latestMagicStackConfigOrder = [self latestMagicStackConfigRank];
  [self.delegate magicStackRankingModel:self
               didGetLatestRankingOrder:_latestMagicStackConfigOrder];
}

- (NSArray<MagicStackModule*>*)latestMagicStackConfigRank {
  NSMutableArray<MagicStackModule*>* magicStackOrder = [NSMutableArray array];
  // Always add Set Up List at the front.
  if ([_setUpListMediator shouldShowSetUpList]) {
    [magicStackOrder addObjectsFromArray:[_setUpListMediator setUpListConfigs]];
  }
  // Currently assume ephemeral cards are always added to the front of the Magic
  // Stack when it can show.
  if (base::FeatureList::IsEnabled(
          segmentation_platform::features::
              kSegmentationPlatformEphemeralCardRanker)) {
    switch (_ephemeralCardToShow) {
      case ContentSuggestionsModuleType::kPriceTrackingPromo:
        if (_priceTrackingPromoMediator &&
            _priceTrackingPromoMediator.priceTrackingPromoItemToShow) {
          [magicStackOrder addObject:_priceTrackingPromoMediator
                                         .priceTrackingPromoItemToShow];
        }
        break;
      case ContentSuggestionsModuleType::kTips:
      case ContentSuggestionsModuleType::kTipsWithProductImage: {
        if (IsTipsMagicStackEnabled()) {
          [magicStackOrder addObject:_tipsMediator.state];
        }
        break;
      }
      default:
        break;
    }
  }
  for (NSNumber* moduleNumber in _magicStackOrderFromSegmentation) {
    ContentSuggestionsModuleType moduleType =
        (ContentSuggestionsModuleType)[moduleNumber intValue];
    switch (moduleType) {
      case ContentSuggestionsModuleType::kMostVisited:
        if (ShouldPutMostVisitedSitesInMagicStack() &&
            [_mostVisitedTilesMediator.mostVisitedConfig
                    .mostVisitedItems count] > 0) {
          [magicStackOrder
              addObject:_mostVisitedTilesMediator.mostVisitedConfig];
        }
        break;
      case ContentSuggestionsModuleType::kTabResumption:
        if (![self shouldShowTabResumption]) {
          break;
        }
        // If ShouldHideIrrelevantModules() is enabled and it is not ranked as
        // the first two modules, do not add it to the Magic Stack.
        if (ShouldHideIrrelevantModules() && [magicStackOrder count] > 1) {
          break;
        }
        [magicStackOrder addObject:_tabResumptionMediator.itemConfig];
        break;
      case ContentSuggestionsModuleType::kSafetyCheck: {
        // Handles adding Safety Check to Magic Stack. Disables/hides if:
        // - Manually disabled or disabled via preferences.
        // - No current or previous issues, to avoid consistently displaying the
        // "All Safe" state and taking up carousel space for other modules.
        // - Irrelevant modules are hidden and it's not the first ranked module.
        BOOL disabled =
            !IsSafetyCheckMagicStackEnabled() ||
            safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
                IsHomeCustomizationEnabled() ? _prefService : _localState);

        if (disabled) {
          base::UmaHistogramEnumeration(
              kIOSSafetyCheckMagicStackHiddenReason,
              IOSSafetyCheckHiddenReason::kManuallyDisabled);
          break;
        }

        int previousIssuesCount = _localState->GetInteger(
            prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount);

        int issuesCount =
            [_safetyCheckMediator.safetyCheckState numberOfIssues];

        BOOL hidden = ShouldHideSafetyCheckModuleIfNoIssues() &&
                      (previousIssuesCount == 0) &&
                      (previousIssuesCount == issuesCount);

        if (hidden) {
          base::UmaHistogramEnumeration(kIOSSafetyCheckMagicStackHiddenReason,
                                        IOSSafetyCheckHiddenReason::kNoIssues);
          break;
        }

        // If ShouldHideIrrelevantModules() is enabled and it is not the first
        // ranked module, do not add it to the Magic Stack.
        if (!ShouldHideIrrelevantModules() || [magicStackOrder count] == 0) {
          [magicStackOrder addObject:_safetyCheckMediator.safetyCheckState];
        }

        break;
      }
      case ContentSuggestionsModuleType::kShortcuts:
        [magicStackOrder addObject:_shortcutsMediator.shortcutsConfig];
        break;
      case ContentSuggestionsModuleType::kParcelTracking:
        if (IsIOSParcelTrackingEnabled() &&
            !IsParcelTrackingDisabled(
                IsHomeCustomizationEnabled() ? _prefService : _localState) &&
            _parcelTrackingMediator.parcelTrackingItemToShow) {
          [magicStackOrder
              addObject:_parcelTrackingMediator.parcelTrackingItemToShow];
        }
        break;
      default:
        // These module types should not have been added by the logic
        // receiving the order list from Segmentation.
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  return magicStackOrder;
}

// Returns NO if client is expecting the order from Segmentation and it has not
// returned yet.
- (BOOL)isMagicStackOrderReady {
    return _magicStackOrderFromSegmentationReceived;
}

// Shows the tab resumption tile with the given `item` configuration.
- (void)showTabResumptionWithItem:(TabResumptionItem*)item {
  if (tab_resumption_prefs::IsLastOpenedURL(item.tabURL, _prefService)) {
    return;
  }

  if (![self isMagicStackOrderReady]) {
    return;
  }
  NSArray<MagicStackModule*>* rank = [self latestMagicStackConfigRank];
  NSUInteger index = [rank indexOfObject:item];
  [self.delegate magicStackRankingModel:self didInsertItem:item atIndex:index];
}

// Returns YES if the tab resumption module should added into the Magic Stack.
- (BOOL)shouldShowTabResumption {
  return IsTabResumptionEnabled() &&
         !tab_resumption_prefs::IsTabResumptionDisabled(
             IsHomeCustomizationEnabled() ? _prefService : _localState) &&
         _tabResumptionMediator.itemConfig;
}

@end
