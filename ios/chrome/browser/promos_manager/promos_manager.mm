// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/promos_manager.h"

#import <Foundation/Foundation.h>

#import <algorithm>
#import <iterator>
#import <map>
#import <numeric>
#import <set>
#import <vector>

#import "base/time/time.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Comparator for descending sort evaluation using std::is_sorted.
bool Compare(promos_manager::Impression a, promos_manager::Impression b) {
  return a.day > b.day;
}

// The number of days since the Unix epoch; one day, in this context, runs
// from UTC midnight to UTC midnight.
int TodaysDay() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InDays();
}

}  // namespace

#pragma mark - PromosManager

#pragma mark - Constructor/Destructor

PromosManager::PromosManager(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

PromosManager::~PromosManager() = default;

#pragma mark - Public

void PromosManager::Init() {
  if (!IsFullscreenPromosManagerEnabled())
    return;

  DCHECK(local_state_);

  const base::Value::List& stored_active_promos =
      local_state_->GetValueList(prefs::kIosPromosManagerActivePromos);

  active_promos_ = stored_active_promos.Clone();
  impression_history_ = ImpressionHistory(
      local_state_->GetValueList(prefs::kIosPromosManagerImpressions));
}

#pragma mark - Private

std::vector<promos_manager::Impression> PromosManager::ImpressionHistory(
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

    impression_history.push_back(promos_manager::Impression(
        promos_manager::PromoForName(*stored_promo), stored_day.value()));
  }

  return impression_history;
}

NSArray<ImpressionLimit*>* PromosManager::PromoImpressionLimits(
    promos_manager::Promo promo) const {
  // TODO(crbug.com/1354665): Return `promo`-specific limits.
  return @[];
}

NSArray<ImpressionLimit*>* PromosManager::GlobalImpressionLimits() const {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    ImpressionLimit* onceEveryTwoDays =
        [[ImpressionLimit alloc] initWithLimit:1 forNumDays:2];
    ImpressionLimit* thricePerWeek = [[ImpressionLimit alloc] initWithLimit:3
                                                                 forNumDays:7];
    limits = @[ onceEveryTwoDays, thricePerWeek ];
  });

  return limits;
}

NSArray<ImpressionLimit*>* PromosManager::GlobalPerPromoImpressionLimits()
    const {
  static NSArray<ImpressionLimit*>* limits;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    ImpressionLimit* oncePerMonth = [[ImpressionLimit alloc] initWithLimit:1
                                                                forNumDays:30];
    limits = @[ oncePerMonth ];
  });

  return limits;
}

int PromosManager::LastSeenDay(
    promos_manager::Promo promo,
    std::vector<promos_manager::Impression>& sorted_impressions) const {
  if (sorted_impressions.empty())
    return promos_manager::kLastSeenDayPromoNotFound;

  DCHECK(std::is_sorted(sorted_impressions.begin(), sorted_impressions.end(),
                        Compare));

  // Find first occurrence of `promo` in list (i.e. find the most recent
  // occurrence of `promo`).
  for (size_t j = 0; j < sorted_impressions.size(); ++j) {
    if (sorted_impressions[j].promo == promo)
      return sorted_impressions[j].day;
  }

  return promos_manager::kLastSeenDayPromoNotFound;
}

bool PromosManager::AnyImpressionLimitTriggered(
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

bool PromosManager::CanShowPromo(
    promos_manager::Promo promo,
    const std::vector<promos_manager::Impression>& valid_impressions) const {
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
    if (curr_impression_index < valid_impressions.size()) {
      promos_manager::Impression curr_impression =
          valid_impressions[curr_impression_index];
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
                                    promo_impression_limits) ||
        AnyImpressionLimitTriggered(most_seen_promo_impression_count,
                                    window_days,
                                    global_per_promo_impression_limits) ||
        AnyImpressionLimitTriggered(total_impression_count, window_days,
                                    global_impression_limits))
      return false;
  }

  return true;
}

std::vector<int> PromosManager::ImpressionCounts(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts;

  for (const auto& [promo, count] : promo_impression_counts)
    counts.push_back(count);

  return counts;
}

int PromosManager::MaxImpressionCount(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts = ImpressionCounts(promo_impression_counts);
  std::vector<int>::iterator max_count_iter =
      std::max_element(counts.begin(), counts.end());
  size_t index = std::distance(counts.begin(), max_count_iter);
  if (index < counts.size())
    return counts[index];
  return 0;
}

int PromosManager::TotalImpressionCount(
    std::map<promos_manager::Promo, int>& promo_impression_counts) const {
  std::vector<int> counts = ImpressionCounts(promo_impression_counts);

  return std::accumulate(counts.begin(), counts.end(), 0);
}

absl::optional<promos_manager::Promo> PromosManager::LeastRecentlyShown(
    const std::set<promos_manager::Promo>& active_promos,
    const std::vector<promos_manager::Impression>& sorted_impressions) const {
  // If there are no active promos, return absl::nullopt.
  // (This is seldom expected to happen, if ever, as Promos Manager will launch
  // with promos_manager::Promo::DefaultBrowser continuously running.)
  if (active_promos.empty())
    return absl::nullopt;

  // When the impression history is empty, no "least recently shown" promo
  // exists—because no promo has ever been shown. In this case,
  // return the first promo in `active_promos`.
  if (sorted_impressions.empty())
    return *active_promos.begin();

  // Loop over the impression history, `sorted_impressions`, and remove promos
  // from `shown` as they are found. Given `sorted_impressions` is sorted from
  // most recent -> least recent, the most recently shown promos will be removed
  // from `shown` first. When `shown` contains just one promo, it must be the
  // least recently shown promo.
  std::set<promos_manager::Promo> shown(active_promos);

  for (promos_manager::Impression impression : sorted_impressions) {
    if (shown.size() == 1)
      return *shown.begin();

    // Given `shown` only contains active promos, impression history for
    // inactive promos will be ignored/skipped; impression history for
    // inactive promos won't affect this method's execution or correctness.
    shown.erase(impression.promo);
  }

  // At this point in method execution, there's been more than one promo that's
  // never been shown. In this case, In this case, return the first unshown
  // promo (a bit awkwardly, these unshown promos are still left in the variable
  // `shown`.)
  DCHECK(!shown.empty());
  return *shown.begin();
}
