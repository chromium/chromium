// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_impl.h"

#import <Foundation/Foundation.h>

#import <iterator>
#import <map>
#import <numeric>
#import <set>
#import <vector>

#import "base/containers/contains.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of days since the Unix epoch; one day, in this context, runs
// from UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

// Conditionally appends `promo` to the list pref `pref_path`. If `promo`
// already exists in the list pref `pref_path`, does nothing. If `promo` doesn't
// exist in the list pref `pref_path`, appends `promo` to the list.
void ConditionallyAppendPromoToPrefList(promos_manager::Promo promo,
                                        const std::string& pref_path,
                                        PrefService* local_state) {
  DCHECK(local_state);

  ScopedListPrefUpdate update(local_state, pref_path);
  base::Value::List& active_promos = update.Get();
  std::string promo_name = promos_manager::NameForPromo(promo);

  // Erase `promo_name` if it already exists in `active_promos`; avoid polluting
  // `active_promos` with duplicate `promo_name` entries.
  active_promos.EraseValue(base::Value(promo_name));

  active_promos.Append(promo_name);
}

}  // namespace

#pragma mark - PromosManagerImpl

#pragma mark - Constructor/Destructor

PromosManagerImpl::PromosManagerImpl(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

PromosManagerImpl::~PromosManagerImpl() = default;

#pragma mark - Public

void PromosManagerImpl::Init() {
  if (!IsFullscreenPromosManagerEnabled())
    return;

  DCHECK(local_state_);

  active_promos_ =
      ActivePromos(local_state_->GetList(prefs::kIosPromosManagerActivePromos));
  single_display_active_promos_ = ActivePromos(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos));
  impression_history_ = ImpressionHistory(
      local_state_->GetList(prefs::kIosPromosManagerImpressions));
}

// Impression history should grow in sorted order. Given this happens on the
// main thread, appending to the end of the impression history list is
// sufficient.
void PromosManagerImpl::RecordImpression(promos_manager::Promo promo) {
  DCHECK(local_state_);

  base::Value::Dict impression;
  impression.Set(promos_manager::kImpressionPromoKey,
                 promos_manager::NameForPromo(promo));
  impression.Set(promos_manager::kImpressionDayKey, TodaysDay());

  ScopedListPrefUpdate update(local_state_,
                              prefs::kIosPromosManagerImpressions);
  update->Append(std::move(impression));

  impression_history_ = ImpressionHistory(
      local_state_->GetList(prefs::kIosPromosManagerImpressions));

  // Auto-deregister `promo` if it's a single-display promo.
  if (single_display_active_promos_.find(promo) !=
      single_display_active_promos_.end()) {
    DeregisterPromo(promo);
  }
}

void PromosManagerImpl::RegisterPromoForContinuousDisplay(
    promos_manager::Promo promo) {
  ConditionallyAppendPromoToPrefList(
      promo, prefs::kIosPromosManagerActivePromos, local_state_);

  active_promos_ =
      ActivePromos(local_state_->GetList(prefs::kIosPromosManagerActivePromos));
}

void PromosManagerImpl::RegisterPromoForSingleDisplay(
    promos_manager::Promo promo) {
  ConditionallyAppendPromoToPrefList(
      promo, prefs::kIosPromosManagerSingleDisplayActivePromos, local_state_);

  single_display_active_promos_ = ActivePromos(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos));
}

void PromosManagerImpl::DeregisterPromo(promos_manager::Promo promo) {
  DCHECK(local_state_);

  ScopedListPrefUpdate active_promos_update(
      local_state_, prefs::kIosPromosManagerActivePromos);
  ScopedListPrefUpdate single_display_promos_update(
      local_state_, prefs::kIosPromosManagerSingleDisplayActivePromos);

  base::Value::List& active_promos = active_promos_update.Get();
  base::Value::List& single_display_promos = single_display_promos_update.Get();

  std::string promo_name = promos_manager::NameForPromo(promo);

  // Erase `promo_name` from the single-display and continuous-display active
  // promos lists.
  active_promos.EraseValue(base::Value(promo_name));
  single_display_promos.EraseValue(base::Value(promo_name));

  active_promos_ =
      ActivePromos(local_state_->GetList(prefs::kIosPromosManagerActivePromos));
  single_display_active_promos_ = ActivePromos(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos));
}

void PromosManagerImpl::InitializePromoImpressionLimits(
    base::small_map<std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
        promo_impression_limits) {
  promo_impression_limits_ = std::move(promo_impression_limits);
}

absl::optional<promos_manager::Promo> PromosManagerImpl::NextPromoForDisplay()
    const {
  // Construct a superset including active (1) single-display and
  // (2) continuous-display promo campaigns.
  std::set<promos_manager::Promo> all_active_promos(active_promos_);

  // Non-destructively insert the single-display promos into
  // `all_active_promos`.
  all_active_promos.insert(single_display_active_promos_.begin(),
                           single_display_active_promos_.end());

  std::vector<promos_manager::Promo> least_recently_shown_promos =
      LeastRecentlyShown(all_active_promos, impression_history_);

  if (least_recently_shown_promos.empty())
    return absl::nullopt;

  for (promos_manager::Promo promo : least_recently_shown_promos)
    if (CanShowPromo(promo, impression_history_))
      return promo;

  return absl::nullopt;
}

#pragma mark - Private

std::vector<promos_manager::Impression> PromosManagerImpl::ImpressionHistory(
    const base::Value::List& stored_impression_history) {
  std::vector<promos_manager::Impression> impression_history;

  for (size_t i = 0; i < stored_impression_history.size(); ++i) {
    const base::Value::Dict& stored_impression =
        stored_impression_history[i].GetDict();
    const std::string* stored_promo =
        stored_impression.FindString(promos_manager::kImpressionPromoKey);
    absl::optional<int> stored_day =
        stored_impression.FindInt(promos_manager::kImpressionDayKey);

    // Skip malformed impression history. (This should almost never happen.)
    if (!stored_promo || !stored_day.has_value())
      continue;

    absl::optional<promos_manager::Promo> promo =
        promos_manager::PromoForName(*stored_promo);

    // Skip malformed impression history. (This should almost never happen.)
    if (!promo.has_value())
      continue;

    impression_history.push_back(
        promos_manager::Impression(promo.value(), stored_day.value()));
  }

  return impression_history;
}

std::set<promos_manager::Promo> PromosManagerImpl::ActivePromos(
    const base::Value::List& stored_active_promos) {
  std::set<promos_manager::Promo> active_promos;

  for (size_t i = 0; i < stored_active_promos.size(); ++i) {
    absl::optional<promos_manager::Promo> promo =
        promos_manager::PromoForName(stored_active_promos[i].GetString());

    // Skip malformed active promos data. (This should almost never happen.)
    if (!promo.has_value())
      continue;

    active_promos.insert(promo.value());
  }

  return active_promos;
}

NSArray<ImpressionLimit*>* PromosManagerImpl::PromoImpressionLimits(
    promos_manager::Promo promo) const {
  auto it = promo_impression_limits_.find(promo);

  if (it == promo_impression_limits_.end())
    return @[];

  return it->second;
}

NSArray<ImpressionLimit*>* PromosManagerImpl::GlobalImpressionLimits() const {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  if (IsSkippingInternalImpressionLimitsEnabled()) {
    return limits;
  }

  dispatch_once(&onceToken, ^{
    ImpressionLimit* onceEveryTwoDays =
        [[ImpressionLimit alloc] initWithLimit:1 forNumDays:2];
    ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                                 forNumDays:7];
    limits = @[ onceEveryTwoDays, thricePerWeek ];
  });

  return limits;
}

NSArray<ImpressionLimit*>* PromosManagerImpl::GlobalPerPromoImpressionLimits()
    const {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  if (IsSkippingInternalImpressionLimitsEnabled()) {
    return limits;
  }

  dispatch_once(&onceToken, ^{
    ImpressionLimit* oncePerMonth = [[ImpressionLimit alloc] initWithLimit:1
                                                                forNumDays:30];
    limits = @[ oncePerMonth ];
  });

  return limits;
}

bool PromosManagerImpl::AnyImpressionLimitTriggered(
    int impression_count,
    int window_days,
    NSArray<ImpressionLimit*>* impression_limits) const {
  for (ImpressionLimit* impression_limit in impression_limits) {
    if (window_days < impression_limit.numDays &&
        impression_count >= impression_limit.numImpressions)
      return true;
  }

  return false;
}

bool PromosManagerImpl::CanShowPromo(
    promos_manager::Promo promo,
    const std::vector<promos_manager::Impression>& sorted_impressions) const {
  // Maintains a map ([promos_manager::Promo] : [current impression count]) for
  // evaluating against GlobalImpressionLimits(),
  // GlobalPerPromoImpressionLimits(), and, if defined, `promo`-specific
  // impression limits
  std::map<promos_manager::Promo, int> promo_impression_counts;

  NSArray<ImpressionLimit*>* promo_impression_limits =
      PromoImpressionLimits(promo);
  NSArray<ImpressionLimit*>* global_per_promo_impression_limits =
      GlobalPerPromoImpressionLimits();
  NSArray<ImpressionLimit*>* global_impression_limits =
      GlobalImpressionLimits();

  int window_start = TodaysDay();
  int window_end =
      (window_start - promos_manager::kNumDaysImpressionHistoryStored) + 1;
  size_t curr_impression_index = 0;

  // Impression limits are defined by a certain number of impressions
  // (int) within a certain window of days (int).
  //
  // This loop starts at TodaysDay() (today) and grows a window, day-by-day, to
  // check against different impression limits.
  //
  // Depending on the size of the window, impression limits may become valid or
  // invalid. For example, if the window covers 7 days, an impression limit of
  // 2-day scope is no longer valid. However, if window covered 1-2 days, an
  // impression limit of 2-day scope is valid.
  for (int curr_day = window_start; curr_day >= window_end; --curr_day) {
    if (curr_impression_index < sorted_impressions.size()) {
      promos_manager::Impression curr_impression =
          sorted_impressions[curr_impression_index];
      // If the current impression matches the current day, add it to
      // `promo_impression_counts`.
      if (curr_impression.day == curr_day) {
        promo_impression_counts[curr_impression.promo]++;
        curr_impression_index++;
      } else {
        // Only check impression limits when counts are incremented: if an
        // impression limit were to be triggered below - but counts weren't
        // incremented above - it wouldve've already been triggered in a
        // previous loop run.
        continue;
      }
    }

    int window_days = window_start - curr_day;
    int promo_impression_count = promo_impression_counts[promo];
    int most_seen_promo_impression_count =
        MaxImpressionCount(promo_impression_counts);
    int total_impression_count = TotalImpressionCount(promo_impression_counts);

    if (AnyImpressionLimitTriggered(promo_impression_count, window_days,
                                    promo_impression_limits)) {
      base::UmaHistogramEnumeration(
          "IOS.PromosManager.Promo.ImpressionLimitEvaluation",
          promos_manager::IOSPromosManagerPromoImpressionLimitEvaluationType::
              kInvalidPromoSpecificImpressionLimitTriggered);

      return false;
    }

    if (AnyImpressionLimitTriggered(most_seen_promo_impression_count,
                                    window_days,
                                    global_per_promo_impression_limits)) {
      base::UmaHistogramEnumeration(
          "IOS.PromosManager.Promo.ImpressionLimitEvaluation",
          promos_manager::IOSPromosManagerPromoImpressionLimitEvaluationType::
              kInvalidPromoAgnosticImpressionLimitTriggered);

      return false;
    }

    if (AnyImpressionLimitTriggered(total_impression_count, window_days,
                                    global_impression_limits)) {
      base::UmaHistogramEnumeration(
          "IOS.PromosManager.Promo.ImpressionLimitEvaluation",
          promos_manager::IOSPromosManagerPromoImpressionLimitEvaluationType::
              kInvalidGlobalImpressionLimitTriggered);

      return false;
    }
  }

  base::UmaHistogramEnumeration(
      "IOS.PromosManager.Promo.ImpressionLimitEvaluation",
      promos_manager::IOSPromosManagerPromoImpressionLimitEvaluationType::
          kValid);

  return true;
}

std::vector<int> PromosManagerImpl::ImpressionCounts(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts;

  for (const auto& [promo, count] : promo_impression_counts)
    counts.push_back(count);

  return counts;
}

int PromosManagerImpl::MaxImpressionCount(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts = ImpressionCounts(promo_impression_counts);
  std::vector<int>::iterator max_count_iter =
      std::max_element(counts.begin(), counts.end());
  size_t index = std::distance(counts.begin(), max_count_iter);
  if (index < counts.size())
    return counts[index];
  return 0;
}

int PromosManagerImpl::TotalImpressionCount(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts = ImpressionCounts(promo_impression_counts);

  return std::accumulate(counts.begin(), counts.end(), 0);
}

std::vector<promos_manager::Promo> PromosManagerImpl::LeastRecentlyShown(
    const std::set<promos_manager::Promo>& active_promos,
    const std::vector<promos_manager::Impression>& sorted_impressions) const {
  std::vector<promos_manager::Promo>
      active_promos_sorted_by_least_recently_shown;

  // If there are no active promos, and no impression history, return an empty
  // array. (This is seldom expected to happen, if ever, as Promos Manager will
  // launch with promos_manager::Promo::DefaultBrowser continuously running.)
  if (active_promos.empty() && sorted_impressions.empty())
    return active_promos_sorted_by_least_recently_shown;

  for (promos_manager::Impression impression : sorted_impressions) {
    // The resulting, sorted array only needs to contain the active promos. Once
    // all active promos are accounted for in
    // `active_promos_sorted_by_least_recently_shown`, we can short-circuit and
    // return `active_promos_sorted_by_least_recently_shown`.
    if (active_promos_sorted_by_least_recently_shown.size() ==
        active_promos.size())
      break;

    // If the current impression's promo already exists in
    // `active_promos_sorted_by_least_recently_shown`, move onto the next
    // impression.
    if (base::Contains(active_promos_sorted_by_least_recently_shown,
                       impression.promo)) {
      continue;
    }

    if (active_promos.count(impression.promo))
      active_promos_sorted_by_least_recently_shown.push_back(impression.promo);
  }

  // It's possible some active promos have never been seen (so no impressions
  // exist for the promo). In that case, add them to the end of the resulting
  // array, before the array is reversed. Those never-before-seen promos will
  // end up at the front of the resulting array after reversal.
  //
  // Never-before-seen promos are considered less recently seen than previously
  // seen promos.
  for (promos_manager::Promo unseen_promo : active_promos) {
    if (!base::Contains(active_promos_sorted_by_least_recently_shown,
                        unseen_promo)) {
      active_promos_sorted_by_least_recently_shown.push_back(unseen_promo);
    }
  }

  std::reverse(active_promos_sorted_by_least_recently_shown.begin(),
               active_promos_sorted_by_least_recently_shown.end());

  return active_promos_sorted_by_least_recently_shown;
}
