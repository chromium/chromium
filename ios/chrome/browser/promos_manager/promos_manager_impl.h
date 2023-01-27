// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_IMPL_H_

#import "ios/chrome/browser/promos_manager/promos_manager.h"

#import <Foundation/Foundation.h>
#import <map>
#import <set>
#import <vector>

#import "base/containers/small_map.h"
#import "base/time/clock.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/impression_limit.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams should not use this directly, use
// promo_manager.h instead.
class PromosManagerImpl : public PromosManager {
 public:
  // Context for a promo registration, internally used.
  struct PromoContext {
    // The promo has had pending status.
    bool was_pending;
  };

  PromosManagerImpl(PrefService* local_state, base::Clock* clock);
  ~PromosManagerImpl() override;

  // `promo`-specific impression limits, if defined. May return an empty
  // NSArray, indicating no promo-specific impression limits were defined for
  // `promo`.
  NSArray<ImpressionLimit*>* PromoImpressionLimits(
      promos_manager::Promo promo) const;

  // Sorts the active promos in the order that they will be displayed.
  std::vector<promos_manager::Promo> SortPromos(
      const std::map<promos_manager::Promo, PromoContext>&
          promos_to_sort_with_context) const;

  // Impression limits that count against all promos.
  NSArray<ImpressionLimit*>* GlobalImpressionLimits() const;

  // Impression limits that count against any given promo.
  NSArray<ImpressionLimit*>* GlobalPerPromoImpressionLimits() const;

  // Loops over the stored impression history (base::Value::List) and returns
  // corresponding a std::vector<promos_manager::Impression>.
  std::vector<promos_manager::Impression> ImpressionHistory(
      const base::Value::List& stored_impression_history);

  // Loops over the stored active promos list (base::Value::List) and returns
  // a corresponding std::set<promos_manager::Promo>.
  std::set<promos_manager::Promo> ActivePromos(
      const base::Value::List& stored_active_promos) const;

  // Initializes the `single_display_pending_promos_`, constructs it from Pref.
  void InitializePendingPromos();

  // Returns true if any impression limit from `impression_limits` is triggered,
  // and false otherwise.
  //
  // At each limit, evaluates the following:
  //
  // (1) Is the current limit valid for evaluation? This is determined by
  // whether or not `window_days` is < the current limit's window.
  //
  // (2) If the limit is valid for evaluation, compare `impression_count` with
  // the current limit's impression count. If `impression_count` >= the current
  // limit's impression count, the limit has been triggered.

  // (3) If the limit is triggered, exits early and returns true. Otherwise,
  // keep going.
  bool AnyImpressionLimitTriggered(
      int impression_count,
      int window_days,
      NSArray<ImpressionLimit*>* impression_limits) const;

  // Algorithm loops over pre-sorted impressions history list. The algorithm
  // assumes `valid_impressions` is sorted by impression day (most recent ->
  // least recent).
  //
  // At each impression, the algorithm asks if either a time-based or
  // time-agnostic impression limit has been met. If so, the algorithm exits
  // early and returns false.
  //
  // If the algorithm reaches its end, no impression limits were hit for
  // `promo`. If so, the algorithm returns true, as it's safe to display
  // `promo`.
  bool CanShowPromo(
      promos_manager::Promo promo,
      const std::vector<promos_manager::Impression>& sorted_impressions) const;

  // Returns a list of impression counts (std::vector<int>) from a promo
  // impression counts map.
  std::vector<int> ImpressionCounts(
      std::map<promos_manager::Promo, int>& promo_impression_counts) const;

  // Returns the greatest impression count (int) from a promo impression counts
  // map.
  int MaxImpressionCount(
      std::map<promos_manager::Promo, int>& promo_impression_counts) const;

  // Returns the total number of impressions (int) from a promo impression
  // counts map.
  int TotalImpressionCount(
      std::map<promos_manager::Promo, int>& promo_impression_counts) const;

  // PromosManager implementation.
  void Init() override;
  void InitializePromoConfigs(PromoConfigsSet promo_configs) override;
  void RecordImpression(promos_manager::Promo promo) override;
  absl::optional<promos_manager::Promo> NextPromoForDisplay() override;
  void RegisterPromoForContinuousDisplay(promos_manager::Promo promo) override;
  void RegisterPromoForSingleDisplay(promos_manager::Promo promo) override;
  void RegisterPromoForSingleDisplay(
      promos_manager::Promo promo,
      base::TimeDelta becomes_active_after_period) override;
  void DeregisterPromo(promos_manager::Promo promo) override;

  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // The time provider.
  const raw_ptr<base::Clock> clock_;

  // The set of currently active, continuous-display promos.
  std::set<promos_manager::Promo> active_promos_;

  // The set of currently active, single-display promos.
  std::set<promos_manager::Promo> single_display_active_promos_;

  // The map from registered single-display pending promos to the time that they
  // can become active.
  std::map<promos_manager::Promo, base::Time> single_display_pending_promos_;

  // The impression history sorted by `day` (most recent -> least recent).
  std::vector<promos_manager::Impression> impression_history_;

  // Promo-specific configuration.
  PromoConfigsSet promo_configs_;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_IMPL_H_
