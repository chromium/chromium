// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_IMPL_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_IMPL_H_

#import "ios/chrome/browser/promos_manager/model/promos_manager.h"

#import <Foundation/Foundation.h>

#import <map>
#import <optional>
#import <set>
#import <vector>

#import "base/containers/small_map.h"
#import "base/memory/raw_ptr.h"
#import "base/time/clock.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"

namespace feature_engagement {
class Tracker;
}

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

  PromosManagerImpl(PrefService* local_state,
                    base::Clock* clock,
                    feature_engagement::Tracker* tracker);
  ~PromosManagerImpl() override;

  // Sorts the active promos in the order that they will be displayed.
  std::vector<promos_manager::Promo> SortPromos(
      const std::map<promos_manager::Promo, PromoContext>&
          promos_to_sort_with_context) const;

  // Loops over the stored active promos list (base::Value::List) and returns
  // a corresponding std::set<promos_manager::Promo>.
  std::set<promos_manager::Promo> ActivePromos(
      const base::Value::List& stored_active_promos) const;

  // Initializes the `single_display_pending_promos_`, constructs it from Pref.
  void InitializePendingPromos();

  // Checks whether a promo can currently be shown using the feature engagement
  // system to check any impression limits. If true, it will mark the promo for
  // display.
  bool CanShowPromo(promos_manager::Promo promo) const;

  // Similar to ```CanShowPromo``` without marking the promo for display.
  bool CanShowPromoWithoutTrigger(promos_manager::Promo promo) const;

  // Returns the corresponding base::Feature for the given Promo.
  const base::Feature* FeatureForPromo(promos_manager::Promo promo) const;

  // Returns the first eligible promo from the given promo queue. If any valid
  // promo is found, it will mark the promo for display.
  std::optional<promos_manager::Promo> GetFirstEligiblePromo(
      const std::vector<promos_manager::Promo>& promo_queue);

  // Returns number of eligible promos in the given promos queue.
  int GetEligiblePromoCount(
      const std::vector<promos_manager::Promo>& promo_queue);

  // PromosManager implementation.
  void Init() override;
  void InitializePromoConfigs(PromoConfigsSet promo_configs) override;
  void DeregisterAfterDisplay(promos_manager::Promo promo) override;
  std::optional<promos_manager::Promo> NextPromoForDisplay() override;
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

  // The feature engagement tracker.
  raw_ptr<feature_engagement::Tracker> tracker_;

  // The set of currently active, continuous-display promos.
  std::set<promos_manager::Promo> active_promos_;

  // The set of currently active, single-display promos.
  std::set<promos_manager::Promo> single_display_active_promos_;

  // The map from registered single-display pending promos to the time that they
  // can become active.
  std::map<promos_manager::Promo, base::Time> single_display_pending_promos_;

  // Promo-specific configuration.
  PromoConfigsSet promo_configs_;

  base::WeakPtrFactory<PromosManagerImpl> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_MODEL_PROMOS_MANAGER_IMPL_H_
