// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"

namespace content {

namespace {

constexpr base::TimeDelta kKAnonymityExpiration = base::Days(7);

}  // namespace

bool IsKAnonymous(const StorageInterestGroup::KAnonymityData& data,
                  const base::Time now) {
  return data.is_k_anonymous && data.last_updated + kKAnonymityExpiration > now;
}

InterestGroupKAnonymityManager::InterestGroupKAnonymityManager(
    InterestGroupManagerImpl* interest_group_manager,
    KAnonymityServiceDelegate* k_anonymity_service)
    : interest_group_manager_(interest_group_manager),
      k_anonymity_service_(k_anonymity_service),
      weak_ptr_factory_(this) {}

InterestGroupKAnonymityManager::~InterestGroupKAnonymityManager() = default;

void InterestGroupKAnonymityManager::QueryKAnonymityForInterestGroup(
    const StorageInterestGroup& storage_group) {
  if (!k_anonymity_service_)
    return;

  std::vector<std::string> ids_to_query;
  base::Time check_time = base::Time::Now();
  base::TimeDelta min_wait = k_anonymity_service_->GetQueryInterval();

  for (const auto& ad : storage_group.bidding_ads_kanon) {
    if (ad.last_updated < check_time - min_wait) {
      ids_to_query.push_back(ad.key);
    }
  }
  for (const auto& ad : storage_group.reporting_ads_kanon) {
    if (ad.last_updated < check_time - min_wait) {
      ids_to_query.push_back(ad.key);
    }
  }

  if (ids_to_query.empty())
    return;

  k_anonymity_service_->QuerySets(
      ids_to_query,
      base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                     weak_ptr_factory_.GetWeakPtr(), ids_to_query,
                     std::move(check_time)));
}

void InterestGroupKAnonymityManager::QuerySetsCallback(
    std::vector<std::string> unhashed_query,
    base::Time update_time,
    std::vector<bool> status) {
  DCHECK_LE(status.size(), unhashed_query.size());
  int size = std::min(unhashed_query.size(), status.size());
  for (int i = 0; i < size; i++) {
    StorageInterestGroup::KAnonymityData data = {unhashed_query[i], status[i],
                                                 update_time};
    interest_group_manager_->UpdateKAnonymity(data);
  }
  // Don't update sets if the request failed.
}

void InterestGroupKAnonymityManager::RegisterAdKeysAsJoined(
    base::flat_set<std::string> keys) {
  for (const auto& key : keys) {
    RegisterIDAsJoined(key);
  }
  // TODO(behamilton): Consider proactively starting a query here to improve the
  // speed that browsers see new ads. We will likely want to rate limit this
  // somehow though.
}

void InterestGroupKAnonymityManager::RegisterIDAsJoined(
    const std::string& key) {
  if (!k_anonymity_service_)
    return;
  if (joins_in_progress.contains(key))
    return;
  joins_in_progress.insert(key);
  interest_group_manager_->GetLastKAnonymityReported(
      key,
      base::BindOnce(&InterestGroupKAnonymityManager::OnGotLastReportedTime,
                     weak_ptr_factory_.GetWeakPtr(), key));
}

void InterestGroupKAnonymityManager::OnGotLastReportedTime(
    std::string key,
    absl::optional<base::Time> last_update_time) {
  if (!last_update_time) {
    joins_in_progress.erase(key);
    return;
  }

  // If it has been long enough since we last joined
  if (base::Time::Now() < last_update_time.value_or(base::Time()) +
                              k_anonymity_service_->GetJoinInterval()) {
    joins_in_progress.erase(key);
    return;
  }

  k_anonymity_service_->JoinSet(
      key, base::BindOnce(&InterestGroupKAnonymityManager::JoinSetCallback,
                          weak_ptr_factory_.GetWeakPtr(), key));
}

void InterestGroupKAnonymityManager::JoinSetCallback(std::string key,
                                                     bool status) {
  if (status) {
    // Update the last reported time if the request succeeded.
    interest_group_manager_->UpdateLastKAnonymityReported(key);
  }
  joins_in_progress.erase(key);
}

}  // namespace content
