// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include <algorithm>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_caching_storage.h"
#include "content/browser/interest_group/interest_group_features.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

namespace content {

namespace {

constexpr base::TimeDelta kKAnonymityExpiration = base::Days(7);

}  // namespace

bool IsKAnonDataExpired(const base::Time last_updated, const base::Time now) {
  bool result = last_updated + kKAnonymityExpiration < now;
  base::UmaHistogramBoolean("Ads.InterestGroup.Auction.KAnonymityDataExpired",
                            result);
  base::UmaHistogramCustomTimes("Ads.InterestGroup.Auction.KAnonymityDataAge",
                                /*sample=*/now - last_updated,
                                /*min=*/base::Seconds(0),
                                /*max=*/base::Days(7),
                                /*buckets=*/50);
  return result;
}

// TODO(orrb): Use caching_storage_ directly instead of
// interest_group_manager_, and remove the InterestGroupManagerImpl methods
// used to delegate to InterestGroupCachingStorage.
InterestGroupKAnonymityManager::InterestGroupKAnonymityManager(
    InterestGroupManagerImpl* interest_group_manager,
    InterestGroupCachingStorage* caching_storage,
    GetKAnonymityServiceDelegateCallback k_anonymity_service_callback)
    : interest_group_manager_(interest_group_manager),
      caching_storage_(caching_storage),
      k_anonymity_service_callback_(k_anonymity_service_callback),
      weak_ptr_factory_(this) {}

InterestGroupKAnonymityManager::~InterestGroupKAnonymityManager() = default;

InterestGroupKAnonymityManager::InProgressQueryState::InProgressQueryState(
    base::Time update_time,
    bool replace_existing_values)
    : update_time(update_time),
      replace_existing_values(replace_existing_values) {}
InterestGroupKAnonymityManager::InProgressQueryState::InProgressQueryState(
    const InProgressQueryState&) = default;
InterestGroupKAnonymityManager::InProgressQueryState::~InProgressQueryState() =
    default;

void InterestGroupKAnonymityManager::QueryKAnonymityOfOwners(
    base::span<const url::Origin> owners) {
  if (!base::FeatureList::IsEnabled(features::kFledgeQueryKAnonymity)) {
    return;
  }
  CHECK(caching_storage_);
  for (const auto& owner : owners) {
    caching_storage_->GetInterestGroupsForOwner(
        owner, base::BindOnce(
                   &InterestGroupKAnonymityManager::OnGotInterestGroupsOfOwner,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void InterestGroupKAnonymityManager::OnGotInterestGroupsOfOwner(
    scoped_refptr<StorageInterestGroups> groups) {
  for (const SingleStorageInterestGroup& group : groups->GetInterestGroups()) {
    InterestGroupKanonUpdateParameter kanon_update(group->last_k_anon_updated);
    kanon_update.hashed_keys = group->interest_group.GetAllKAnonKeys();
    QueryKAnonymityData(blink::InterestGroupKey(group->interest_group.owner,
                                                group->interest_group.name),
                        kanon_update);
  }
}

void InterestGroupKAnonymityManager::QueryKAnonymityData(
    const blink::InterestGroupKey& interest_group_key,
    const InterestGroupKanonUpdateParameter& k_anon_data) {
  if (!base::FeatureList::IsEnabled(features::kFledgeQueryKAnonymity)) {
    return;
  }

  KAnonymityServiceDelegate* k_anonymity_service =
      k_anonymity_service_callback_.Run();
  if (!k_anonymity_service) {
    return;
  }

  base::Time check_time = base::Time::Now();
  bool replace_existing_values = true;
  base::TimeDelta min_wait = k_anonymity_service->GetQueryInterval();
  if (k_anon_data.update_time > check_time - min_wait) {
    // The last update happened recently but we should still update the
    // newly-added keys if needed.
    replace_existing_values = false;
  }

  // TODO(orrb): All of the logic distinguishing new keys from keys being
  // refreshed may be obsoleted by the k-anonymity keys cache queried below,
  // since keys are only held in the cache for a TTL that matches the query
  // interval, so that the cache can mimic the limitation of how frequently
  // each key is fetched from the k-anonymity server.
  const std::vector<std::string>& ids_to_query =
      replace_existing_values ? k_anon_data.hashed_keys
                              : k_anon_data.newly_added_hashed_keys;
  if (ids_to_query.size() == 0) {
    return;
  }
  InProgressQueryState new_query_state(check_time, replace_existing_values);
  auto in_progress_query_it =
      queries_in_progress_.insert({interest_group_key, new_query_state});
  if (!in_progress_query_it.second) {
    if (replace_existing_values ||
        in_progress_query_it.first->second.update_time <
            check_time - min_wait) {
      // This new request replaces all existing requests or the outstanding
      // request is outdated.
      in_progress_query_it.first->second = new_query_state;
    }
  }

  if (base::FeatureList::IsEnabled(features::kFledgeCacheKAnonHashedKeys) &&
      caching_storage_ != nullptr) {
    caching_storage_->LoadPositiveHashedKAnonymityKeysFromCache(
        ids_to_query, check_time,
        base::BindOnce(
            &InterestGroupKAnonymityManager::FetchUncachedKAnonymityData,
            weak_ptr_factory_.GetWeakPtr(),
            in_progress_query_it.first->second.update_time,
            interest_group_key));
  } else {
    FetchUncachedKAnonymityData(
        in_progress_query_it.first->second.update_time, interest_group_key,
        InterestGroupStorage::KAnonymityCacheResponse(
            /*_positive_hashed_keys_from_cache=*/{},
            /*_ids_to_query_from_server=*/ids_to_query));
  }
}

void InterestGroupKAnonymityManager::FetchUncachedKAnonymityData(
    base::Time update_time,
    const blink::InterestGroupKey& interest_group_key,
    InterestGroupStorage::KAnonymityCacheResponse cache_response) {
  auto it = queries_in_progress_.find(interest_group_key);
  if (it == queries_in_progress_.end() ||
      update_time != it->second.update_time) {
    return;
  }

  // If all of the keys we need were fetched from the cache, update k-anonymous
  // keys on the interest group and clean up the `InProgressQueryState`.
  if (cache_response.ids_to_query_from_server.empty()) {
    interest_group_manager_->UpdateKAnonymity(
        interest_group_key, cache_response.positive_hashed_keys_from_cache,
        update_time, it->second.replace_existing_values);
    queries_in_progress_.erase(it);
    return;
  }

  it->second.positive_hashed_keys_from_received_responses =
      std::move(cache_response.positive_hashed_keys_from_cache);

  KAnonymityServiceDelegate* k_anonymity_service =
      k_anonymity_service_callback_.Run();
  if (!k_anonymity_service) {
    // Cancel the update.
    queries_in_progress_.erase(interest_group_key);
    return;
  }

  std::vector<std::string> ids_to_query_in_next_batch;
  for (std::string& key : cache_response.ids_to_query_from_server) {
    ids_to_query_in_next_batch.emplace_back(std::move(key));

    if (ids_to_query_in_next_batch.size() >= kQueryBatchSizeLimit) {
      it->second.remaining_responses++;
      k_anonymity_service->QuerySets(
          ids_to_query_in_next_batch,
          base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                         weak_ptr_factory_.GetWeakPtr(),
                         ids_to_query_in_next_batch, it->second.update_time,
                         interest_group_key));
      ids_to_query_in_next_batch.clear();
    }
  }

  if (ids_to_query_in_next_batch.empty()) {
    return;
  }
  it->second.remaining_responses++;
  k_anonymity_service->QuerySets(
      ids_to_query_in_next_batch,
      base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                     weak_ptr_factory_.GetWeakPtr(), ids_to_query_in_next_batch,
                     it->second.update_time, interest_group_key));
}

void InterestGroupKAnonymityManager::QuerySetsCallback(
    std::vector<std::string> hashed_query,
    base::Time update_time,
    const blink::InterestGroupKey& interest_group_key,
    std::vector<bool> status) {
  DCHECK_LE(status.size(), hashed_query.size());
  if (status.empty()) {
    // There was an error. Cancel the update.
    queries_in_progress_.erase(interest_group_key);
    return;
  }

  auto it = queries_in_progress_.find(interest_group_key);
  if (it == queries_in_progress_.end() ||
      update_time != it->second.update_time) {
    return;
  }

  std::vector<std::string> negative_hashed_keys_from_received_responses;
  int size = std::min(hashed_query.size(), status.size());
  for (int i = 0; i < size; i++) {
    if (status[i]) {
      it->second.positive_hashed_keys_from_received_responses.emplace_back(
          hashed_query[i]);
    } else {
      negative_hashed_keys_from_received_responses.emplace_back(
          hashed_query[i]);
    }
  }

  if (base::FeatureList::IsEnabled(features::kFledgeCacheKAnonHashedKeys) &&
      caching_storage_ != nullptr) {
    caching_storage_->WriteHashedKAnonymityKeysToCache(
        it->second.positive_hashed_keys_from_received_responses,
        negative_hashed_keys_from_received_responses, update_time);
  }

  it->second.remaining_responses--;
  if (it->second.remaining_responses == 0) {
    interest_group_manager_->UpdateKAnonymity(
        interest_group_key,
        it->second.positive_hashed_keys_from_received_responses, update_time,
        it->second.replace_existing_values);
    queries_in_progress_.erase(it);
  }
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
  if (joins_in_progress_.contains(hashed_key)) {
    return;
  }
  joins_in_progress_.insert(hashed_key);
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
    joins_in_progress_.erase(hashed_key);
    return;
  }

  // If it has been long enough since we last joined
  if (base::Time::Now() < last_update_time.value_or(base::Time()) +
                              k_anonymity_service->GetJoinInterval()) {
    joins_in_progress_.erase(hashed_key);
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
  joins_in_progress_.erase(hashed_key);
}

}  // namespace content
