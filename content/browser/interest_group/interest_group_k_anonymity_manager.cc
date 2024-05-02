// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

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
    GetKAnonymityServiceDelegateCallback k_anonymity_service_callback)
    : interest_group_manager_(interest_group_manager),
      k_anonymity_service_callback_(k_anonymity_service_callback),
      weak_ptr_factory_(this) {}

InterestGroupKAnonymityManager::~InterestGroupKAnonymityManager() = default;

void InterestGroupKAnonymityManager::QueryKAnonymityForInterestGroup(
    const blink::InterestGroupKey& interest_group_key) {
  interest_group_manager_->GetKAnonymityDataForUpdate(
      interest_group_key,
      base::BindOnce(&InterestGroupKAnonymityManager::QueryKAnonymityData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InterestGroupKAnonymityManager::QueryKAnonymityData(
    const std::vector<StorageInterestGroup::KAnonymityData>& k_anon_data) {
  KAnonymityServiceDelegate* k_anonymity_service =
      k_anonymity_service_callback_.Run();
  if (!k_anonymity_service) {
    return;
  }

  std::vector<std::string> ids_to_query;
  base::Time check_time = base::Time::Now();
  base::TimeDelta min_wait = k_anonymity_service->GetQueryInterval();

  for (const StorageInterestGroup::KAnonymityData& k_anon_data_item :
       k_anon_data) {
    if (k_anon_data_item.last_updated < check_time - min_wait) {
      ids_to_query.push_back(k_anon_data_item.hashed_key);
    }

    if (ids_to_query.size() >= kQueryBatchSizeLimit) {
      k_anonymity_service->QuerySets(
          ids_to_query,
          base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                         weak_ptr_factory_.GetWeakPtr(), ids_to_query,
                         check_time));
      ids_to_query.clear();
    }
  }

  if (ids_to_query.empty())
    return;

  k_anonymity_service->QuerySets(
      ids_to_query,
      base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                     weak_ptr_factory_.GetWeakPtr(), ids_to_query,
                     std::move(check_time)));
}

void InterestGroupKAnonymityManager::QuerySetsCallback(
    std::vector<std::string> hashed_query,
    base::Time update_time,
    std::vector<bool> status) {
  DCHECK_LE(status.size(), hashed_query.size());
  int size = std::min(hashed_query.size(), status.size());
  for (int i = 0; i < size; i++) {
    StorageInterestGroup::KAnonymityData data = {hashed_query[i], status[i],
                                                 update_time};
    interest_group_manager_->UpdateKAnonymity(data);
  }
  // Don't update sets if the request failed.
}

void InterestGroupKAnonymityManager::RegisterAdKeysAsJoined(
    base::flat_set<std::string> hashed_keys) {
  for (const auto& key : hashed_keys) {
    RegisterIDAsJoined(key);
  }
  // TODO(behamilton): Consider proactively starting a query here to improve the
  // speed that browsers see new ads. We will likely want to rate limit this
  // somehow though.
}

void InterestGroupKAnonymityManager::RegisterIDAsJoined(
    const std::string& hashed_key) {
  if (joins_in_progress.contains(hashed_key)) {
    return;
  }
  joins_in_progress.insert(hashed_key);
  interest_group_manager_->GetLastKAnonymityReported(
      hashed_key,
      base::BindOnce(&InterestGroupKAnonymityManager::OnGotLastReportedTime,
                     weak_ptr_factory_.GetWeakPtr(), hashed_key));
}

void InterestGroupKAnonymityManager::OnGotLastReportedTime(
    std::string hashed_key,
    std::optional<base::Time> last_update_time) {
  KAnonymityServiceDelegate* k_anonymity_service =
      k_anonymity_service_callback_.Run();
  if (!k_anonymity_service) {
    return;
  }
  if (!last_update_time) {
    joins_in_progress.erase(hashed_key);
    return;
  }

  // If it has been long enough since we last joined
  if (base::Time::Now() < last_update_time.value_or(base::Time()) +
                              k_anonymity_service->GetJoinInterval()) {
    joins_in_progress.erase(hashed_key);
    return;
  }

  k_anonymity_service->JoinSet(
      hashed_key,
      base::BindOnce(&InterestGroupKAnonymityManager::JoinSetCallback,
                     weak_ptr_factory_.GetWeakPtr(), hashed_key));
}

void InterestGroupKAnonymityManager::JoinSetCallback(std::string hashed_key,
                                                     bool status) {
  if (status) {
    // Update the last reported time if the request succeeded.
    interest_group_manager_->UpdateLastKAnonymityReported(hashed_key);
  }
  joins_in_progress.erase(hashed_key);
}

}  // namespace content
