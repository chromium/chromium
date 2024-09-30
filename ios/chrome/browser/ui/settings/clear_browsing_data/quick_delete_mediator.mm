// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/browsing_data/core/cookie_or_cache_deletion_choice.h"
#import "components/browsing_data/core/counters/autofill_counter.h"
#import "components/browsing_data/core/counters/history_counter.h"
#import "components/browsing_data/core/counters/passwords_counter.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/model/tabs_counter.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_consumer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_presentation_commands.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using browsing_data::DeleteBrowsingDataDialogAction;
using browsing_data::kDeleteBrowsingDataDialogHistogram;
using browsing_data::TimePeriod;

// Delay to observe when triggering further actions after browsing data removal
// has completed so the progress UI state is not flashed.
constexpr base::TimeDelta kBrowsingDataRemoveCompletionDelay = base::Seconds(1);

// Records the Privacy.DeleteBrowsingData.Dialog histogram for `period`.
void RecordTimePeriodPrefChange(TimePeriod period) {
  switch (period) {
    case TimePeriod::LAST_15_MINUTES:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kLast15MinutesSelected);
      break;
    case TimePeriod::LAST_HOUR:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kLastHourSelected);
      break;
    case TimePeriod::LAST_DAY:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kLastDaySelected);
      break;
    case TimePeriod::LAST_WEEK:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kLastWeekSelected);
      break;
    case TimePeriod::FOUR_WEEKS:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kLastFourWeeksSelected);
      break;
    case TimePeriod::ALL_TIME:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kAllTimeSelected);
      break;
    case TimePeriod::OLDER_THAN_30_DAYS:
      base::UmaHistogramEnumeration(
          kDeleteBrowsingDataDialogHistogram,
          DeleteBrowsingDataDialogAction::kOlderThan30DaysSelected);
      break;
  }
}

// Record the UserDeletedCookieOrCacheFromDialog histogram.
void RecordCookieOrCacheDeletedFromDialogHistogram(
    BrowsingDataRemoveMask remove_mask) {
  browsing_data::CookieOrCacheDeletionChoice choice;
  if (IsRemoveDataMaskSet(remove_mask, BrowsingDataRemoveMask::REMOVE_CACHE)) {
    choice =
        IsRemoveDataMaskSet(remove_mask, BrowsingDataRemoveMask::REMOVE_COOKIES)
            ? browsing_data::CookieOrCacheDeletionChoice::kBothCookiesAndCache
            : browsing_data::CookieOrCacheDeletionChoice::kOnlyCache;
  } else {
    choice =
        IsRemoveDataMaskSet(remove_mask, BrowsingDataRemoveMask::REMOVE_COOKIES)
            ? browsing_data::CookieOrCacheDeletionChoice::kOnlyCookies
            : browsing_data::CookieOrCacheDeletionChoice::
                  kNeitherCookiesNorCache;
  }
  base::UmaHistogramEnumeration(
      "History.ClearBrowsingData.UserDeletedCookieOrCacheFromDialog", choice);
}

}  // namespace

@interface QuickDeleteMediator () <IdentityManagerObserverBridgeDelegate,
                                   PrefObserverDelegate>
@end

@implementation QuickDeleteMediator {
  raw_ptr<PrefService> _prefs;
  BrowsingDataCounterWrapperProducer* _counterWrapperProducer;
  raw_ptr<BrowsingDataRemover> _browsingDataRemover;
  raw_ptr<DiscoverFeedService> _discoverFeedService;

  // The currently selected time range in the UI. Only saved into the
  // `kDeleteTimePeriod` pref when the deletion is triggered.
  browsing_data::TimePeriod _selectedTimeRange;

  // Summaries based on the results returned by `_counters`. If they're nil, it
  // means that the counter for the browsing data type in `_counters` has not
  // yet returned.
  NSString* _browsingHistorySummary;
  NSString* _tabsSummary;
  NSString* _passwordsSummary;
  NSString* _addressesSummary;
  NSString* _paymentMethodsSummary;
  NSString* _suggestionsSummary;

  // Set of `BrowsingDataCounter`s used to create the summaries.
  std::set<std::unique_ptr<BrowsingDataCounterWrapper>> _counters;

  // Tracks if the placeholder summary has already been dispatched to the
  // `_consumer` in order to avoid uncessary calls in
  // `dispatchPlaceholderSummary`.
  bool _placeholderSummaryWasDispatched;

  // Holds the tabs information used and returned by the `TabsCounter`. Will be
  // used as part of the removal of browsing data to avoid reading from disk
  // twice.
  tabs_closure_util::WebStateIDToTime _cachedTabsInfo;

  // Used to get the current sign-in state of the primary account.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  BOOL _canPerformTabsClosureAnimation;

  // Indicates if deletion has been triggered before.
  BOOL _deletionTriggered;
}

- (instancetype)initWithPrefs:(PrefService*)prefs
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)counterWrapperProducer
                       identityManager:(signin::IdentityManager*)identityManager
                   browsingDataRemover:(BrowsingDataRemover*)browsingDataRemover
                   discoverFeedService:(DiscoverFeedService*)discoverFeedService
        canPerformTabsClosureAnimation:(BOOL)canPerformTabsClosureAnimation {
  if ((self = [super init])) {
    _prefs = prefs;
    _counterWrapperProducer = counterWrapperProducer;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _browsingDataRemover = browsingDataRemover;
    _discoverFeedService = discoverFeedService;

    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));

    _selectedTimeRange = static_cast<browsing_data::TimePeriod>(
        _prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod));

    // Start observing preferences.
    [self observePreferences];

    _canPerformTabsClosureAnimation = canPerformTabsClosureAnimation;
  }
  return self;
}

- (void)setConsumer:(id<QuickDeleteConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  [_consumer setTimeRange:_selectedTimeRange];

  BOOL shouldShowFooter =
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  [_consumer setShouldShowFooter:shouldShowFooter];
  [_consumer
      setHistorySelection:_prefs->GetBoolean(
                              browsing_data::prefs::kDeleteBrowsingHistory)];
  [_consumer
      setTabsSelection:_prefs->GetBoolean(browsing_data::prefs::kCloseTabs)];
  [_consumer setSiteDataSelection:_prefs->GetBoolean(
                                      browsing_data::prefs::kDeleteCookies)];
  [_consumer
      setCacheSelection:_prefs->GetBoolean(browsing_data::prefs::kDeleteCache)];
  [_consumer setPasswordsSelection:_prefs->GetBoolean(
                                       browsing_data::prefs::kDeletePasswords)];
  [_consumer setAutofillSelection:_prefs->GetBoolean(
                                      browsing_data::prefs::kDeleteFormData)];

  [self createCounters];
  [self restartCounters];
}

- (void)disconnect {
  _prefObserverBridge.reset();
  _prefChangeRegistrar.RemoveAll();
  _counters.clear();
  _counterWrapperProducer = nil;
  _prefs = nil;
  _identityManagerObserver.reset();
  _identityManager = nil;
  _browsingDataRemover = nullptr;
  _discoverFeedService = nullptr;
  _deletionTriggered = NO;
}

#pragma mark - QuickDeleteMutator

- (void)timeRangeSelected:(browsing_data::TimePeriod)timeRange {
  if (_selectedTimeRange == timeRange) {
    return;
  }

  _selectedTimeRange = timeRange;
}

- (void)triggerDeletion {
  browsing_data::RecordDeleteBrowsingDataAction(
      browsing_data::DeleteBrowsingDataAction::kClearBrowsingDataDialog);

  // TODO(crbug.com/365776279): Monitor if deletion can be double triggered.
  DUMP_WILL_BE_CHECK(!_deletionTriggered);
  _deletionTriggered = YES;

  // Only update the time range pref on deletion.
  _prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                     static_cast<int>(_selectedTimeRange));
  RecordTimePeriodPrefChange(_selectedTimeRange);

  [_consumer deletionInProgress];

  BrowsingDataRemoveMask removeMask = BrowsingDataRemoveMask::REMOVE_NOTHING;

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory)) {
    removeMask |= BrowsingDataRemoveMask::REMOVE_HISTORY;

    // If browsing History will be cleared set the kLastClearBrowsingDataTime.
    // TODO(crbug.com/40693626): This pref is used by the Feed to prevent the
    // showing of customized content after history has been cleared. We might
    // want to create a specific Pref for this.
    _prefs->SetInt64(browsing_data::prefs::kLastClearBrowsingDataTime,
                     base::Time::Now().ToTimeT());

    _discoverFeedService->BrowsingHistoryCleared();
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    removeMask |= BrowsingDataRemoveMask::REMOVE_SITE_DATA;
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteCache)) {
    removeMask |= BrowsingDataRemoveMask::REMOVE_CACHE;
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeletePasswords)) {
    removeMask |= BrowsingDataRemoveMask::REMOVE_PASSWORDS;
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteFormData)) {
    removeMask |= BrowsingDataRemoveMask::REMOVE_FORM_DATA;
  }

  bool shouldCloseTabs = _prefs->GetBoolean(browsing_data::prefs::kCloseTabs);

  CHECK((removeMask != BrowsingDataRemoveMask::REMOVE_NOTHING) ||
        shouldCloseTabs);

  // If we cannot perform the tabs closure animation, then close the tabs when
  // deleting the other data.
  if (shouldCloseTabs && !_canPerformTabsClosureAnimation) {
    _browsingDataRemover->SetCachedTabsInfo(_cachedTabsInfo);
    removeMask |= BrowsingDataRemoveMask::CLOSE_TABS;
  }

  base::Time beginTime =
      browsing_data::CalculateBeginDeleteTime(_selectedTimeRange);
  base::Time endTime =
      browsing_data::CalculateEndDeleteTime(_selectedTimeRange);
  BrowsingDataRemover::RemovalParams params{
      .show_activity_indicator =
          BrowsingDataRemover::ActivityIndicatorPolicy::kNoIndicator,
  };

  base::OnceClosure removeBrowsingDataCompletion;

  // If we can perform the tabs closure animation, then don't close the tabs
  // right away, but perform the animation which will eventually close the tabs.
  // Also delay the reload of tabs that is needed when site data and cache is
  // delete to the end of the tabs being closed. This avoids reloading and
  // consequently writting history of tabs that will be closed.
  if (shouldCloseTabs && _canPerformTabsClosureAnimation) {
    params.reload_web_states =
        BrowsingDataRemover::WebStatesReloadPolicy::kNoReload;
    BOOL forceWebStatesReload =
        IsRemoveDataMaskSet(removeMask, BrowsingDataRemoveMask::REMOVE_CACHE);

    __weak __typeof(self) weakSelf = self;
    removeBrowsingDataCompletion = base::BindOnce(
        [](__typeof(self) strongSelf, base::Time beginTime, base::Time endTime,
           bool forceWebStatesReload) {
          [strongSelf
              triggerTabsClosureAnimationWithBeginTime:beginTime
                                               endTime:endTime
                                  forceWebStatesReload:forceWebStatesReload];
        },
        weakSelf, beginTime, endTime, forceWebStatesReload);
  } else {
    __weak __typeof(self.consumer) weakConsumer = self.consumer;
    removeBrowsingDataCompletion = base::BindOnce(
        [](__typeof(self.consumer) strongConsumer) {
          [strongConsumer deletionFinished];
        },
        weakConsumer);
  }

  base::OnceClosure delayedCompletion = base::BindOnce(
      [](base::OnceClosure completion) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, std::move(completion),
            kBrowsingDataRemoveCompletionDelay);
      },
      std::move(removeBrowsingDataCompletion));

  // If we have nothing to remove in the in progress UI, then already trigger
  // the completion block.
  if (removeMask == BrowsingDataRemoveMask::REMOVE_NOTHING) {
    CHECK(shouldCloseTabs);
    std::move(delayedCompletion).Run();
  } else {
    _browsingDataRemover->RemoveInRange(beginTime, endTime, removeMask,
                                        std::move(delayedCompletion), params);
  }

  RecordCookieOrCacheDeletedFromDialogHistogram(removeMask);
}

- (void)updateHistorySelection:(BOOL)selected {
  BOOL current_state =
      _prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory);
  if (current_state == selected) {
    return;
  }

  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      selected ? DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOn
               : DeleteBrowsingDataDialogAction::kBrowsingHistoryToggledOff);
  _prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, selected);
}

- (void)updateTabsSelection:(BOOL)selected {
  BOOL current_state = _prefs->GetBoolean(browsing_data::prefs::kCloseTabs);
  if (current_state == selected) {
    return;
  }

  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      selected ? DeleteBrowsingDataDialogAction::kTabsToggledOn
               : DeleteBrowsingDataDialogAction::kTabsToggledOff);
  _prefs->SetBoolean(browsing_data::prefs::kCloseTabs, selected);
}

- (void)updateSiteDataSelection:(BOOL)selected {
  BOOL current_state = _prefs->GetBoolean(browsing_data::prefs::kDeleteCookies);
  if (current_state == selected) {
    return;
  }

  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      selected ? DeleteBrowsingDataDialogAction::kSiteDataToggledOn
               : DeleteBrowsingDataDialogAction::kSiteDataToggledOff);
  _prefs->SetBoolean(browsing_data::prefs::kDeleteCookies, selected);
}

- (void)updateCacheSelection:(BOOL)selected {
  BOOL current_state = _prefs->GetBoolean(browsing_data::prefs::kDeleteCache);
  if (current_state == selected) {
    return;
  }

  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      selected ? DeleteBrowsingDataDialogAction::kCacheToggledOn
               : DeleteBrowsingDataDialogAction::kCacheToggledOff);
  _prefs->SetBoolean(browsing_data::prefs::kDeleteCache, selected);
}

- (void)updatePasswordsSelection:(BOOL)selected {
  BOOL current_state =
      _prefs->GetBoolean(browsing_data::prefs::kDeletePasswords);
  if (current_state == selected) {
    return;
  }

  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      selected ? DeleteBrowsingDataDialogAction::kPasswordsToggledOn
               : DeleteBrowsingDataDialogAction::kPasswordsToggledOff);
  _prefs->SetBoolean(browsing_data::prefs::kDeletePasswords, selected);
}

- (void)updateAutofillSelection:(BOOL)selected {
  BOOL current_state =
      _prefs->GetBoolean(browsing_data::prefs::kDeleteFormData);
  if (current_state == selected) {
    return;
  }

  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      selected ? DeleteBrowsingDataDialogAction::kAutofillToggledOn
               : DeleteBrowsingDataDialogAction::kAutofillToggledOff);
  _prefs->SetBoolean(browsing_data::prefs::kDeleteFormData, selected);
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the sign-in state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      [self.consumer setShouldShowFooter:YES];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self.consumer setShouldShowFooter:NO];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == browsing_data::prefs::kDeleteTimePeriod) {
    _selectedTimeRange = static_cast<browsing_data::TimePeriod>(
        _prefs->GetInteger(browsing_data::prefs::kDeleteTimePeriod));
  }

  if (preferenceName == browsing_data::prefs::kDeleteTimePeriod ||
      preferenceName == browsing_data::prefs::kDeleteBrowsingHistory ||
      preferenceName == browsing_data::prefs::kCloseTabs ||
      preferenceName == browsing_data::prefs::kDeleteCookies ||
      preferenceName == browsing_data::prefs::kDeleteCache ||
      preferenceName == browsing_data::prefs::kDeletePasswords ||
      preferenceName == browsing_data::prefs::kDeleteFormData) {
    [self restartCounters];
    [self updateConsumerSelectionForPref:preferenceName];
    return;
  }

  DCHECK(false) << "Unxpected clear browsing data item type.";
}

#pragma mark - Private

// Trigger the tab closure animation along with the actual closure of the
// WebStates within [`beginTime`, `endTime`[ and indicates if reloading tabs is
// necessary after the deletion has happen.
- (void)triggerTabsClosureAnimationWithBeginTime:(base::Time)beginTime
                                         endTime:(base::Time)endTime
                            forceWebStatesReload:(BOOL)forceWebStatesReload {
  CHECK(_canPerformTabsClosureAnimation);
  [_presentationHandler
      triggerTabsClosureAnimationWithBeginTime:beginTime
                                       endTime:endTime
                                cachedTabsInfo:_cachedTabsInfo
                          forceWebStatesReload:forceWebStatesReload];
}

// Creates counters for browsing history, passwords and form data browsing data
// types. These counters when triggered by `restartCounters` will lead to an
// update of the browsing data summary in the ViewController.
- (void)createCounters {
  [self createCounter:browsing_data::prefs::kDeleteBrowsingHistory];
  [self createCounter:browsing_data::prefs::kCloseTabs];
  [self createCounter:browsing_data::prefs::kDeleteCache];
  [self createCounter:browsing_data::prefs::kDeletePasswords];
  [self createCounter:browsing_data::prefs::kDeleteFormData];
}

// Creates a counter for the browsing data type defined by the `prefName`.
- (void)createCounter:(std::string)prefName {
  __weak __typeof(self) weakSelf = self;
  std::unique_ptr<BrowsingDataCounterWrapper> counter = [_counterWrapperProducer
      createCounterWrapperWithPrefName:prefName
                             beginTime:browsing_data::CalculateBeginDeleteTime(
                                           _selectedTimeRange)
                      updateUiCallback:
                          base::BindRepeating(^(
                              const browsing_data::BrowsingDataCounter::Result&
                                  result) {
                            [weakSelf updateSummaryWith:&result];
                            [weakSelf updateResultOnConsumer:&result];
                          })];
  if (counter != nullptr) {
    _counters.insert(std::move(counter));
  }
}

// Restarts the counters created in `createdCounters` with `_selectedTimeRange`.
// Restarting the counters results on the browsing data summary being updated in
// the ViewController.
- (void)restartCounters {
  _browsingHistorySummary = nil;
  _tabsSummary = nil;
  _passwordsSummary = nil;
  _addressesSummary = nil;
  _paymentMethodsSummary = nil;
  _suggestionsSummary = nil;

  _cachedTabsInfo.clear();
  _placeholderSummaryWasDispatched = NO;

  for (std::set<std::unique_ptr<BrowsingDataCounterWrapper>>::iterator it =
           _counters.begin();
       it != _counters.end(); ++it) {
    // Setting a new begin time, restarts the counter.
    (*it)->SetBeginTime(
        browsing_data::CalculateBeginDeleteTime(_selectedTimeRange));
  }
}

// Dispatches the placeholder browsing data summary if it hasn't already been
// dispatched to the `_consumer`.
- (void)dispatchPlaceholderSummary {
  if (_placeholderSummaryWasDispatched) {
    return;
  }
  _placeholderSummaryWasDispatched = YES;
  [_consumer setBrowsingDataSummary:l10n_util::GetNSString(
                                        IDS_CLEAR_BROWSING_DATA_CALCULATING)];
}

// Updates the summary for the browsing data type in `result`. Dispatches the
// complete browsing data summary to the ViewController if all browsing data
// types with counters have finished.
//
// There is a garantee that `BrowsingDataCounter` will eventually trigger (up
// until a maximum delay has elapsed) its callback with a finished
// `BrowsingDataCounter::Result`.
- (void)updateSummaryWith:
    (const browsing_data::BrowsingDataCounter::Result*)result {
  if (!result->Finished()) {
    // A placeholder summary should be displayed since the counter still needs a
    // bit more time to return the actual result.
    [self dispatchPlaceholderSummary];
    return;
  }

  std::string prefName = result->source()->GetPrefName();
  if (prefName == browsing_data::prefs::kDeleteBrowsingHistory) {
    _browsingHistorySummary = [self
        browsingHistorySummary:
            static_cast<const browsing_data::HistoryCounter::HistoryResult*>(
                result)];
  } else if (prefName == browsing_data::prefs::kCloseTabs) {
    const TabsCounter::TabsResult* tabsResult =
        static_cast<const TabsCounter::TabsResult*>(result);
    _cachedTabsInfo = tabsResult->cached_tabs_info();
    _tabsSummary = [self tabsSummary:tabsResult];
  } else if (prefName == browsing_data::prefs::kDeletePasswords) {
    _passwordsSummary = [self
        passwordsSummary:static_cast<const browsing_data::PasswordsCounter::
                                         PasswordsResult*>(result)];
  } else if (prefName == browsing_data::prefs::kDeleteFormData) {
    const browsing_data::AutofillCounter::AutofillResult* autofillResult =
        static_cast<const browsing_data::AutofillCounter::AutofillResult*>(
            result);
    _addressesSummary = [self addressesSummary:autofillResult];
    _paymentMethodsSummary = [self paymentMethodsSummary:autofillResult];
    _suggestionsSummary = [self suggestionsSummary:autofillResult];
  } else if (prefName == browsing_data::prefs::kDeleteCache) {
    // Do nothing as we don't display the calculated cache result in the summary
    // on the bottom sheet.
    // TODO(crbug.com/353211728): Construct the summary on the VC using the new
    // result methods provided on the mediator.
  } else {
    NOTREACHED();
  }

  // All `BrowsingDataCounter` will eventually return with a finished
  // `BrowsingDataCounter::Result`. Meaning that eventually this if condition
  // will evaluate to true and a non-placeholder browsing data summary will be
  // dispatched.
  if (_browsingHistorySummary && _tabsSummary && _passwordsSummary &&
      _addressesSummary && _paymentMethodsSummary && _suggestionsSummary) {
    [self dispatchBrowsingDataSummary];
  }
}

// Dispatches the updated browsing data summary to the ViewController.
- (void)dispatchBrowsingDataSummary {
  NSMutableArray<NSString*>* summaryItems = [[NSMutableArray alloc] init];

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteBrowsingHistory)) {
    if (_browsingHistorySummary && _browsingHistorySummary.length > 0) {
      [summaryItems addObject:_browsingHistorySummary];
    }
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kCloseTabs)) {
    if (_tabsSummary && _tabsSummary.length > 0) {
      [summaryItems addObject:_tabsSummary];
    }
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteCookies)) {
    [summaryItems
        addObject:l10n_util::GetNSString(
                      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITE_DATA)];
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteCache)) {
    [summaryItems
        addObject:l10n_util::GetNSString(
                      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_CACHED_FILES)];
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeletePasswords)) {
    if (_passwordsSummary && _passwordsSummary.length > 0) {
      [summaryItems addObject:_passwordsSummary];
    }
  }

  if (_prefs->GetBoolean(browsing_data::prefs::kDeleteFormData)) {
    if (_addressesSummary && _addressesSummary.length > 0) {
      [summaryItems addObject:_addressesSummary];
    }

    if (_paymentMethodsSummary && _paymentMethodsSummary.length > 0) {
      [summaryItems addObject:_paymentMethodsSummary];
    }

    if (_suggestionsSummary && _suggestionsSummary.length > 0) {
      [summaryItems addObject:_suggestionsSummary];
    }
  }

  if (!summaryItems.count) {
    [_consumer setBrowsingDataSummary:
                   l10n_util::GetNSString(
                       IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_NO_DATA)];
    return;
  }

  NSString* summary =
      [summaryItems componentsJoinedByString:
                        l10n_util::GetNSString(
                            IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SEPARATOR)];

  _placeholderSummaryWasDispatched = NO;
  NSString* summaryWithFirstLetterCapitalized = [[[summary substringToIndex:1]
      uppercaseString] stringByAppendingString:[summary substringFromIndex:1]];
  [_consumer setBrowsingDataSummary:summaryWithFirstLetterCapitalized];
}

// Returns the browsing history summary based on `result`. If the count of
// browsing history entries in `result ` is less than 1, then returns an empty
// string.
- (NSString*)browsingHistorySummary:
    (const browsing_data::HistoryCounter::HistoryResult*)result {
  CHECK(result);
  browsing_data::BrowsingDataCounter::ResultInt historyCount = result->Value();
  if (historyCount < 1) {
    return @"";
  }

  return result->has_synced_visits()
             ? l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES_SYNCED,
                   historyCount)
             : l10n_util::GetPluralNSStringF(
                   IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SITES, historyCount);
}

// Returns the tabs summary based on `result`. If the count of tabs in
// `result ` is less than 1, then returns an empty string.
- (NSString*)tabsSummary:(const TabsCounter::TabsResult*)result {
  browsing_data::BrowsingDataCounter::ResultInt tabsCount = result->Value();

  if (tabsCount < 1) {
    return @"";
  }

  return l10n_util::GetPluralNSStringF(
      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_TABS, tabsCount);
}

// Returns the passwords summary based on `result`. If the count of passwords in
// `result ` is less than 1, then returns an empty string.
- (NSString*)passwordsSummary:
    (const browsing_data::PasswordsCounter::PasswordsResult*)result {
  browsing_data::BrowsingDataCounter::ResultInt passwordCount =
      result->Value() + result->account_passwords();

  if (passwordCount < 1) {
    return @"";
  }

  return l10n_util::GetPluralNSStringF(
      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PASSWORDS, passwordCount);
}

// Returns the addresses summary based on `result`. If the count of addresses in
// `result ` is less than 1, then returns an empty string.
- (NSString*)addressesSummary:
    (const browsing_data::AutofillCounter::AutofillResult*)result {
  browsing_data::AutofillCounter::ResultInt addressesCount =
      result->num_addresses();

  if (addressesCount < 1) {
    return @"";
  }

  return l10n_util::GetPluralNSStringF(
      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_ADRESSES, addressesCount);
}

// Returns the payment methods summary based on `result`. If the count of
// payment methods in `result ` is less than 1, then returns an empty string.
- (NSString*)paymentMethodsSummary:
    (const browsing_data::AutofillCounter::AutofillResult*)result {
  browsing_data::AutofillCounter::ResultInt paymentMethodsCount =
      result->num_credit_cards();

  if (paymentMethodsCount < 1) {
    return @"";
  }

  return l10n_util::GetPluralNSStringF(
      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_PAYMENT_METHODS,
      paymentMethodsCount);
}

// Returns the suggestions summary based on `result`. If the count of
// suggestions in `result ` is less than 1, then returns an empty string.
- (NSString*)suggestionsSummary:
    (const browsing_data::AutofillCounter::AutofillResult*)result {
  browsing_data::AutofillCounter::ResultInt suggestionCount = result->Value();

  if (suggestionCount < 1) {
    return @"";
  }

  return l10n_util::GetPluralNSStringF(
      IDS_IOS_DELETE_BROWSING_DATA_SUMMARY_SUGGESTIONS, suggestionCount);
}

- (void)observePreferences {
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteTimePeriod, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteBrowsingHistory, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kCloseTabs, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteCookies, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteCache, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeletePasswords, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteFormData, &_prefChangeRegistrar);
}

- (void)updateResultOnConsumer:
    (const browsing_data::BrowsingDataCounter::Result*)result {
  std::string prefName = result->source()->GetPrefName();
  NSString* summary =
      quick_delete_util::GetCounterTextFromResult(*result, _selectedTimeRange);

  if (prefName == browsing_data::prefs::kDeleteBrowsingHistory) {
    [_consumer setHistorySummary:summary];
    return;
  }

  if (prefName == browsing_data::prefs::kCloseTabs) {
    [_consumer setTabsSummary:summary];
    return;
  }

  if (prefName == browsing_data::prefs::kDeleteCache) {
    [_consumer setCacheSummary:summary];
    return;
  }

  if (prefName == browsing_data::prefs::kDeletePasswords) {
    [_consumer setPasswordsSummary:summary];
    return;
  }

  if (prefName == browsing_data::prefs::kDeleteFormData) {
    [_consumer setAutofillSummary:summary];
    return;
  }
}

- (void)updateConsumerSelectionForPref:(const std::string&)preferenceName {
  if (preferenceName == browsing_data::prefs::kDeleteTimePeriod) {
    [_consumer setTimeRange:_selectedTimeRange];
    // Maybe update cache summary.
    return;
  }

  if (preferenceName == browsing_data::prefs::kDeleteBrowsingHistory) {
    [_consumer setHistorySelection:_prefs->GetBoolean(preferenceName)];
    return;
  }

  if (preferenceName == browsing_data::prefs::kCloseTabs) {
    [_consumer setTabsSelection:_prefs->GetBoolean(preferenceName)];
    return;
  }

  if (preferenceName == browsing_data::prefs::kDeleteCookies) {
    [_consumer setSiteDataSelection:_prefs->GetBoolean(preferenceName)];
    return;
  }

  if (preferenceName == browsing_data::prefs::kDeleteCache) {
    [_consumer setCacheSelection:_prefs->GetBoolean(preferenceName)];
    return;
  }

  if (preferenceName == browsing_data::prefs::kDeletePasswords) {
    [_consumer setPasswordsSelection:_prefs->GetBoolean(preferenceName)];
    return;
  }

  if (preferenceName == browsing_data::prefs::kDeleteFormData) {
    [_consumer setAutofillSelection:_prefs->GetBoolean(preferenceName)];
    return;
  }
}

@end
