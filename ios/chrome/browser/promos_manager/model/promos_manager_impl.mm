// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/promos_manager/model/promos_manager_impl.h"

#import <Foundation/Foundation.h>

#import <algorithm>
#import <iterator>
#import <map>
#import <numeric>
#import <optional>
#import <set>
#import <vector>

#import "base/containers/contains.h"
#import "base/feature_list.h"
#import "base/json/values_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/features.h"
#import "ios/chrome/browser/promos_manager/model/impression_limit.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

using promos_manager::Promo;

namespace {

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

}  // namespace

#pragma mark - PromosManagerImpl

#pragma mark - Constructor/Destructor

PromosManagerImpl::PromosManagerImpl(PrefService* local_state,
                                     base::Clock* clock,
                                     feature_engagement::Tracker* tracker)
    : local_state_(local_state), clock_(clock), tracker_(tracker) {
  DCHECK(local_state_);
  DCHECK(clock_);
}

PromosManagerImpl::~PromosManagerImpl() = default;

#pragma mark - Public

void PromosManagerImpl::Init() {
  DCHECK(local_state_);

  active_promos_ =
      ActivePromos(local_state_->GetList(prefs::kIosPromosManagerActivePromos));
  single_display_active_promos_ = ActivePromos(
      local_state_->GetList(prefs::kIosPromosManagerSingleDisplayActivePromos));

  InitializePendingPromos();
}

void PromosManagerImpl::DeregisterAfterDisplay(promos_manager::Promo promo) {
  // Auto-deregister single display promos.
  // Edge case: Possible to remove two instances of promo in
  // `single_display_active_promos_` and `single_display_pending_promos_` that
  // match the same type.
  if (base::Contains(single_display_active_promos_, promo) ||
      base::Contains(single_display_pending_promos_, promo)) {
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
}

// Determines which promo to display next.
// Candidates are from active promos and the pending promos that can become
// active at the time this function is called. Coordinate with other internal
// functions to rank and validate the candidates.
std::optional<promos_manager::Promo> PromosManagerImpl::NextPromoForDisplay() {
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
    return std::nullopt;
  }

  // Get eligible promo count before ```GetFirstEligiblePromo``` otherwise the
  // count might not be accurate.
  int valid_promo_count = GetEligiblePromoCount(sorted_promos);
  if (valid_promo_count == 0) {
    return std::nullopt;
  }

  std::optional<promos_manager::Promo> first_promo_opt =
      GetFirstEligiblePromo(sorted_promos);
  if (!first_promo_opt) {
    return std::nullopt;
  }

  // If there is a promo eligible for display then record number of valid promos
  // in the queue. This is to understand how often eligible promos don't get
  // picked because of other promos.
  base::UmaHistogramExactLinear("IOS.PromosManager.EligiblePromosInQueueCount",
                                valid_promo_count,
                                static_cast<int>(Promo::kMaxValue) + 1);

  return first_promo_opt;
}

std::set<promos_manager::Promo> PromosManagerImpl::ActivePromos(
    const base::Value::List& stored_active_promos) const {
  std::set<promos_manager::Promo> active_promos;

  for (size_t i = 0; i < stored_active_promos.size(); ++i) {
    std::optional<promos_manager::Promo> promo =
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
    std::optional<promos_manager::Promo> promo =
        promos_manager::PromoForName(name);
    // Skip malformed promo data.
    if (!promo.has_value()) {
      continue;
    }
    std::optional<base::Time> becomes_active_time = ValueToTime(value);
    // Skip malformed time data.
    if (!becomes_active_time.has_value()) {
      continue;
    }
    single_display_pending_promos_[promo.value()] = becomes_active_time.value();
  }
}

bool PromosManagerImpl::CanShowPromoWithoutTrigger(
    promos_manager::Promo promo) const {
  const base::Feature* feature = FeatureForPromo(promo);
  if (!feature) {
    return false;
  }
  return tracker_->WouldTriggerHelpUI(*feature);
}

bool PromosManagerImpl::CanShowPromo(promos_manager::Promo promo) const {
  const base::Feature* feature = FeatureForPromo(promo);
  if (!feature) {
    return false;
  }
  return tracker_->ShouldTriggerHelpUI(*feature);
}

const base::Feature* PromosManagerImpl::FeatureForPromo(
    promos_manager::Promo promo) const {
  auto it = promo_configs_.find(promo);
  if (it == promo_configs_.end()) {
    return nil;
  }

  return it->feature_engagement_feature;
}

// Sort the promos in the order that they will be displayed.
// Based on the Promo's context and type.
std::vector<promos_manager::Promo> PromosManagerImpl::SortPromos(
    const std::map<promos_manager::Promo, PromoContext>&
        promos_to_sort_with_context) const {
  std::vector<std::pair<promos_manager::Promo, PromoContext>>
      promos_list_to_sort;

  for (const auto& it : promos_to_sort_with_context) {
    promos_list_to_sort.push_back(
        std::pair<promos_manager::Promo, PromoContext>(it.first, it.second));
  }

  // The order: PostRestoreSignIn types are shown first, then Promos with
  // pending state, then Promos without pending state. For promos without
  // pending state, those never before shown come before those that have been
  // shown before.
  auto compare_promo = [this](
                           std::pair<promos_manager::Promo, PromoContext> lhs,
                           std::pair<promos_manager::Promo, PromoContext> rhs) {
    // PostRestoreDefaultBrowser comes first.
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
    // Post-default browser abandonment promo comes next.
    if (lhs.first == Promo::PostDefaultAbandonment) {
      return true;
    }
    if (rhs.first == Promo::PostDefaultAbandonment) {
      return false;
    }
    // prefer the promo with pending state to the other without.
    if (lhs.second.was_pending && !rhs.second.was_pending) {
      return true;
    }
    if (!lhs.second.was_pending && rhs.second.was_pending) {
      return false;
    }

    // Check Feature Engagement Tracker data for promos.
    const base::Feature* lhs_feature = FeatureForPromo(lhs.first);
    const base::Feature* rhs_feature = FeatureForPromo(rhs.first);
    if (!lhs_feature && !rhs_feature) {
      return lhs.first < rhs.first;
    } else if (!rhs_feature) {
      return true;
    } else if (!lhs_feature) {
      return false;
    }
    if (!tracker_->IsInitialized()) {
      return lhs.first < rhs.first;
    }
    // Prefer the promo that has not been shown to the
    // one that has.
    bool lhs_shown = tracker_->HasEverTriggered(*lhs_feature, true);
    bool rhs_shown = tracker_->HasEverTriggered(*rhs_feature, true);
    if (!lhs_shown && rhs_shown) {
      return true;
    }
    if (lhs_shown && !rhs_shown) {
      return false;
    }
    return lhs.first < rhs.first;
  };

  sort(promos_list_to_sort.begin(), promos_list_to_sort.end(), compare_promo);

  std::vector<promos_manager::Promo> sorted_promos;
  for (const auto& it : promos_list_to_sort) {
    sorted_promos.push_back(it.first);
  }

  return sorted_promos;
}

std::optional<promos_manager::Promo> PromosManagerImpl::GetFirstEligiblePromo(
    const std::vector<promos_manager::Promo>& promo_queue) {
  for (promos_manager::Promo promo : promo_queue) {
    if (CanShowPromo(promo)) {
      return promo;
    }
  }
  return std::nullopt;
}

int PromosManagerImpl::GetEligiblePromoCount(
    const std::vector<promos_manager::Promo>& promo_queue) {
  int count = 0;
  for (promos_manager::Promo promo : promo_queue) {
    if (CanShowPromoWithoutTrigger(promo)) {
      count++;
    }
  }
  return count;
}
