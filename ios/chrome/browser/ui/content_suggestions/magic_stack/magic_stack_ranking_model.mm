// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "components/prefs/pref_service.h"
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "ios/chrome/browser/ntp_tiles/model/tab_resumption/tab_resumption_prefs.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/most_visited_tiles_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_config.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/shortcuts_mediator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_metrics_recorder.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_ranking_model_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_item.h"
#import "ios/chrome/browser/ui/content_suggestions/parcel_tracking/parcel_tracking_mediator.h"
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
#import "ios/chrome/browser/ui/ntp/home_start_data_source.h"

@interface MagicStackRankingModel () <MostVisitedTilesMediatorDelegate,
                                      ParcelTrackingMediatorDelegate,
                                      SafetyCheckMagicStackMediatorDelegate,
                                      SetUpListMediatorAudience,
                                      ShortcutsMediatorDelegate,
                                      TabResumptionHelperDelegate>
// For testing-only
@property(nonatomic, assign) BOOL hasReceivedMagicStackResponse;
@end

@implementation MagicStackRankingModel {
  segmentation_platform::SegmentationPlatformService* _segmentationService;
  PrefService* _prefService;
  PrefService* _localState;
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
  ShortcutsMediator* _shortcutsMediator;
  SafetyCheckMagicStackMediator* _safetyCheckMediator;
  base::TimeTicks ranking_fetch_start_time_;
}

- (instancetype)initWithSegmentationService:
                    (segmentation_platform::SegmentationPlatformService*)
                        segmentationService
                                prefService:(PrefService*)prefService
                                 localState:(PrefService*)localState
                            moduleMediators:(NSArray*)moduleMediators {
  self = [super init];
  if (self) {
    _segmentationService = segmentationService;
    _prefService = prefService;
    _localState = localState;

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
      } else if ([mediator
                     isKindOfClass:[SafetyCheckMagicStackMediator class]]) {
        _safetyCheckMediator =
            static_cast<SafetyCheckMagicStackMediator*>(mediator);
        _safetyCheckMediator.delegate = self;
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
  _shortcutsMediator = nil;
  _safetyCheckMediator = nil;
}

#pragma mark - Public

- (void)fetchLatestMagicStackRanking {
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
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
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

  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
                            ContentSuggestionsModuleType::kSafetyCheck);
  [self.delegate magicStackRankingModel:self
                          didRemoveItem:_safetyCheckMediator.safetyCheckState];
}

#pragma mark - TabResumptionHelperDelegate

- (void)tabResumptionHelperDidReceiveItem {
  CHECK(IsTabResumptionEnabled());
  if (tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
    return;
  }

  [self showTabResumptionWithItem:_tabResumptionMediator.itemConfig];
}

- (void)tabResumptionHelperDidReconfigureItem {
  if (tab_resumption_prefs::IsTabResumptionDisabled(_localState)) {
    return;
  }
  TabResumptionItem* item = _tabResumptionMediator.itemConfig;
  [self.delegate magicStackRankingModel:self didReconfigureItem:item];
}

- (void)removeTabResumptionModule {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
                            ContentSuggestionsModuleType::kTabResumption);
  [self.delegate magicStackRankingModel:self
                          didRemoveItem:_tabResumptionMediator.itemConfig];
}

#pragma mark - ParcelTrackingMediatorDelegate

- (void)newParcelsAvailable {
  MagicStackModule* item = _parcelTrackingMediator.parcelTrackingItemToShow;
  NSArray<MagicStackModule*>* rank = [self latestMagicStackConfigRank];
  NSUInteger index = [rank indexOfObject:item];
  [self.delegate magicStackRankingModel:self didInsertItem:item atIndex:index];
}

- (void)parcelTrackingDisabled {
  UMA_HISTOGRAM_ENUMERATION(kMagicStackModuleDisabledHistogram,
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
  _magicStackOrderFromSegmentationReceived = NO;
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
      case ContentSuggestionsModuleType::kSafetyCheck:
        if (!IsSafetyCheckMagicStackEnabled() ||
            safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
                _localState)) {
          break;
        }
        // If ShouldHideIrrelevantModules() is enabled and it is not the first
        // ranked module, do not add it to the Magic Stack.
        if (!ShouldHideIrrelevantModules() || [magicStackOrder count] == 0) {
          [magicStackOrder addObject:_safetyCheckMediator.safetyCheckState];
        }
        break;
      case ContentSuggestionsModuleType::kShortcuts:
        [magicStackOrder addObject:_shortcutsMediator.shortcutsConfig];
        break;
      case ContentSuggestionsModuleType::kParcelTracking:
        if (IsIOSParcelTrackingEnabled() &&
            !IsParcelTrackingDisabled(
                GetApplicationContext()->GetLocalState()) &&
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
         !tab_resumption_prefs::IsTabResumptionDisabled(_localState) &&
         _tabResumptionMediator.itemConfig;
}

@end
