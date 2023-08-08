// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager_impl.h"

#import <Foundation/Foundation.h>

#import <algorithm>
#import <iterator>
#import <map>
#import <numeric>
#import <set>
#import <vector>

#import "base/containers/contains.h"
#import "base/json/values_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "ios/chrome/browser/promos_manager/promos_manager_event_exporter.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

using promos_manager::Promo;

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

  std::string promo_name = promos_manager::NameForPromo(promo);

  // Erase `promo_name` if it already exists in `active_promos`; avoid polluting
  // `active_promos` with duplicate `promo_name` entries.
  update->EraseValue(base::Value(promo_name));

  update->Append(promo_name);
}

// Returns true if the first impression is more recent and false otherwise.
bool CompareImpressions(promos_manager::Impression impression1,
                        promos_manager::Impression impression2) {
  return impression1.day > impression2.day;
}

}  // namespace

#pragma mark - PromosManagerImpl

#pragma mark - Constructor/Destructor

PromosManagerImpl::PromosManagerImpl(PrefService* local_state,
                                     base::Clock* clock,
                                     feature_engagement::Tracker* tracker,
                                     PromosManagerEventExporter* event_exporter)
    : local_state_(local_state),
      clock_(clock),
      tracker_(tracker),
      event_exporter_(event_exporter) {
  DCHECK(local_state_);
  DCHECK(clock_);
  if (ShouldPromosManagerUseFET()) {
    tracker_->AddOnInitializedCallback(base::BindOnce(
        &PromosManagerImpl::OnFeatureEngagementTrackerInitialized,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

PromosManagerImpl::~PromosManagerImpl() = default;

void PromosManagerImpl::RefreshImpressionHistoryFromPrefs() {
  impression_history_ = ImpressionHistory(
      local_state_->GetList(prefs::kIosPromosManagerImpressions));
  // Sort impressions from most recent to least recent.
  std::sort(impression_history_.begin(), impression_history_.end(),
            CompareImpressions);
}

#pragma mark - Public

void PromosManagerImpl::Init() {
  DCHECK(local_state_);

  active_promos_ =
      ActivePromos(local_state_->GetList(prefs::kIosPromosManagerActivePromos));
  single_display_active_promos_ = ActivePromos(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos));

  InitializePendingPromos();
  RefreshImpressionHistoryFromPrefs();
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
  impression.Set(
      promos_manager::kImpressionFeatureEngagementMigrationCompletedKey,
      ShouldPromosManagerUseFET());

  ScopedListPrefUpdate update(local_state_,
                              prefs::kIosPromosManagerImpressions);

  update->Append(std::move(impression));

  RefreshImpressionHistoryFromPrefs();

  // Auto-deregister `promo`.
  // Edge case: Possible to remove two instances of promo in
  // `single_display_active_promos_` and `single_display_pending_promos_` that
  // match the same type.
  if (base::Contains(single_display_active_promos_, promo) ||
      base::Contains(single_display_pending_promos_, promo)) {
    DeregisterPromo(promo);
  }
}

void PromosManagerImpl::OnFeatureEngagementTrackerInitialized(bool success) {
  CHECK(ShouldPromosManagerUseFET());
  if (success) {
    // Loading the tracker may cause event migration to take place, so re-load
    // the impressions in case they have changed.
    RefreshImpressionHistoryFromPrefs();
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

void PromosManagerImpl::RegisterPromoForSingleDisplay(
    promos_manager::Promo promo,
    base::TimeDelta becomes_active_after_period) {
  DCHECK(local_state_);

  // update the pending promos saved in pref.
  ScopedDictPrefUpdate pending_promos_update(
      local_state_, prefs::kIosPromosManagerSingleDisplayPendingPromos);
  std::string promo_name = promos_manager::NameForPromo(promo);
  base::Time becomes_active_time = clock_->Now() + becomes_active_after_period;
  pending_promos_update->Set(promo_name,
                             base::TimeToValue(becomes_active_time));

  // keep the in-memory pending promos up-to-date to avoid reading from pref
  // frequently.
  single_display_pending_promos_[promo] = becomes_active_time;
}

void PromosManagerImpl::DeregisterPromo(promos_manager::Promo promo) {
  DCHECK(local_state_);

  ScopedListPrefUpdate active_promos_update(
      local_state_, prefs::kIosPromosManagerActivePromos);
  ScopedListPrefUpdate single_display_promos_update(
      local_state_, prefs::kIosPromosManagerSingleDisplayActivePromos);
  ScopedDictPrefUpdate pending_promos_update(
      local_state_, prefs::kIosPromosManagerSingleDisplayPendingPromos);

  std::string promo_name = promos_manager::NameForPromo(promo);

  // Erase `promo_name` from the single-display and continuous-display active
  // promos lists.
  active_promos_update->EraseValue(base::Value(promo_name));
  single_display_promos_update->EraseValue(base::Value(promo_name));
  pending_promos_update->Remove(promo_name);

  active_promos_ =
      ActivePromos(local_state_->GetList(prefs::kIosPromosManagerActivePromos));
  single_display_active_promos_ = ActivePromos(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos));
  single_display_pending_promos_.erase(promo);
}

void PromosManagerImpl::InitializePromoConfigs(PromoConfigsSet promo_configs) {
  promo_configs_ = std::move(promo_configs);
  if (event_exporter_) {
    event_exporter_->InitializePromoConfigs(promo_configs);
  }
}

// Determines which promo to display next.
// Candidates are from active promos and the pending promos that can become
// active at the time this function is called. Coordinate with other internal
// functions to rank and validate the candidates.
absl::optional<promos_manager::Promo> PromosManagerImpl::NextPromoForDisplay() {
  // Construct a map with the promo from (1) single-display and
  // (2) continuous-display promo campaigns. (3) single-display pending promos
  // that has become active, as keys. The value is the context that will be used
  // for ranking purpose.
  std::map<promos_manager::Promo, PromoContext> active_promos_with_context;
  for (const auto& promo : active_promos_) {
    active_promos_with_context[promo] = PromoContext{
        .was_pending = false,
    };
  }

  // Non-destructively insert the single-display promos into
  // `all_active_promos`.
  for (const auto& promo : single_display_active_promos_) {
    active_promos_with_context[promo] = PromoContext{
        .was_pending = false,
    };
  }

  // Insert the pending promos that have become active.
  // Possibly overrides the same promo from `single_display_active_promos_`, as
  // the pending promo has higher priority in current use cases.
  const base::Time now = clock_->Now();
  for (const auto& [promo, time] : single_display_pending_promos_) {
    if (time < now) {
      active_promos_with_context[promo] = PromoContext{
          .was_pending = true,
      };
    }
  }

  std::vector<promos_manager::Promo> sorted_promos =
      SortPromos(active_promos_with_context);

  if (sorted_promos.empty()) {
    return absl::nullopt;
  }

  for (promos_manager::Promo promo : sorted_promos) {
    if (CanShowPromo(promo, impression_history_))
      return promo;
  }

  return absl::nullopt;
}

std::vector<promos_manager::Impression> PromosManagerImpl::ImpressionHistory(
    const base::Value::List& stored_impression_history) {
  std::vector<promos_manager::Impression> impression_history;

  for (size_t i = 0; i < stored_impression_history.size(); ++i) {
    const base::Value::Dict& stored_impression =
        stored_impression_history[i].GetDict();
    absl::optional<promos_manager::Impression> impression =
        promos_manager::ImpressionFromDict(stored_impression);
    if (!impression) {
      continue;
    }

    impression_history.push_back(impression.value());
  }

  return impression_history;
}

std::set<promos_manager::Promo> PromosManagerImpl::ActivePromos(
    const base::Value::List& stored_active_promos) const {
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

// Should only be called in the `init` to avoid excessive reading from pref.
void PromosManagerImpl::InitializePendingPromos() {
  DCHECK(local_state_);

  single_display_pending_promos_.clear();

  const base::Value::Dict& stored_pending_promos =
      local_state_->GetDict(prefs::kIosPromosManagerSingleDisplayPendingPromos);

  for (const auto [name, value] : stored_pending_promos) {
    absl::optional<promos_manager::Promo> promo =
        promos_manager::PromoForName(name);
    // Skip malformed promo data.
    if (!promo.has_value()) {
      continue;
    }
    absl::optional<base::Time> becomes_active_time = ValueToTime(value);
    // Skip malformed time data.
    if (!becomes_active_time.has_value()) {
      continue;
    }
    single_display_pending_promos_[promo.value()] = becomes_active_time.value();
  }
}

NSArray<ImpressionLimit*>* PromosManagerImpl::PromoImpressionLimits(
    promos_manager::Promo promo) const {
  auto it = promo_configs_.find(promo);

  if (it == promo_configs_.end()) {
    return @[];
  }

  return it->impression_limits;
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
  if (ShouldPromosManagerUseFET()) {
    return CanShowPromoUsingFeatureEngagementTracker(promo);
  }
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
    int total_impression_count = TotalImpressionCount(promo_impression_counts);

    if (AnyImpressionLimitTriggered(promo_impression_count, window_days,
                                    promo_impression_limits)) {
      base::UmaHistogramEnumeration(
          "IOS.PromosManager.Promo.ImpressionLimitEvaluation",
          promos_manager::IOSPromosManagerPromoImpressionLimitEvaluationType::
              kInvalidPromoSpecificImpressionLimitTriggered);

      return false;
    }

    if (AnyImpressionLimitTriggered(promo_impression_count, window_days,
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

bool PromosManagerImpl::CanShowPromoUsingFeatureEngagementTracker(
    promos_manager::Promo promo) const {
  auto it = promo_configs_.find(promo);
  if (it == promo_configs_.end()) {
    return false;
  }

  const base::Feature* feature = it->feature_engagement_feature;
  if (!feature) {
    return false;
  }
  return tracker_->ShouldTriggerHelpUI(*feature);
}

std::vector<int> PromosManagerImpl::ImpressionCounts(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts;

  for (const auto& [promo, count] : promo_impression_counts)
    counts.push_back(count);

  return counts;
}

int PromosManagerImpl::TotalImpressionCount(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts = ImpressionCounts(promo_impression_counts);

  return std::accumulate(counts.begin(), counts.end(), 0);
}

// Sort the promos in the order that they will be displayed.
// Based on the Promo's context, type, and the recently shown time.
std::vector<promos_manager::Promo> PromosManagerImpl::SortPromos(
    const std::map<promos_manager::Promo, PromoContext>&
        promos_to_sort_with_context) const {
  std::vector<std::pair<promos_manager::Promo, PromoContext>>
      promos_list_to_sort;

  for (const auto& it : promos_to_sort_with_context) {
    promos_list_to_sort.push_back(
        std::pair<promos_manager::Promo, PromoContext>(it.first, it.second));
  }

  const std::vector<promos_manager::Impression>& impression_history =
      impression_history_;

  // The order: PostRestoreSignIn types are shown first, then Promos with
  // pending state, then Promos without pending state in least-recently-shown
  // order.
  auto compare_promo = [&impression_history](
                           std::pair<promos_manager::Promo, PromoContext> lhs,
                           std::pair<promos_manager::Promo, PromoContext> rhs) {
    // Choice types are to be displayed first.
    if (lhs.first == Promo::Choice) {
      return true;
    }
    if (rhs.first == Promo::Choice) {
      return false;
    }
    // PostRestoreDefaultBrowser comes next.
    if (lhs.first == Promo::PostRestoreDefaultBrowserAlert) {
      return true;
    }
    if (rhs.first == Promo::PostRestoreDefaultBrowserAlert) {
      return false;
    }
    // PostRestoreSignIn types come next.
    if (lhs.first == Promo::PostRestoreSignInFullscreen ||
        lhs.first == Promo::PostRestoreSignInAlert) {
      return true;
    }
    if (rhs.first == Promo::PostRestoreSignInFullscreen ||
        rhs.first == Promo::PostRestoreSignInAlert) {
      return false;
    }
    // prefer the promo with pending state to the other without.
    if (lhs.second.was_pending && !rhs.second.was_pending) {
      return true;
    }
    if (!lhs.second.was_pending && rhs.second.was_pending) {
      return false;
    }
    // Tied after comparing the type and pending state, break using the most
    // recently shown times, prefer the promo that was shown less recently.
    auto lhs_impression =
        std::find_if(impression_history.begin(), impression_history.end(),
                     [lhs](promos_manager::Impression impression) {
                       return impression.promo == lhs.first;
                     });
    // If the promo is unseen, make it show first.
    if (lhs_impression == impression_history.end()) {
      return true;
    }
    auto rhs_impression =
        std::find_if(impression_history.begin(), impression_history.end(),
                     [rhs](promos_manager::Impression impression) {
                       return impression.promo == rhs.first;
                     });
    if (rhs_impression == impression_history.end()) {
      return false;
    }
    // Both promos are seen. `impression_history` is in the most recently seen
    // order. larger iterator = less recently seen = displayed first
    return lhs_impression > rhs_impression;
  };

  sort(promos_list_to_sort.begin(), promos_list_to_sort.end(), compare_promo);

  std::vector<promos_manager::Promo> sorted_promos;
  for (const auto& it : promos_list_to_sort) {
    sorted_promos.push_back(it.first);
  }

  return sorted_promos;
}
