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

bool IsKAnonDataExpired(const base::Time last_updated, const base::Time now) {
  return last_updated + kKAnonymityExpiration < now;
}

InterestGroupKAnonymityManager::InterestGroupKAnonymityManager(
    InterestGroupManagerImpl* interest_group_manager,
    GetKAnonymityServiceDelegateCallback k_anonymity_service_callback)
    : interest_group_manager_(interest_group_manager),
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

void InterestGroupKAnonymityManager::QueryKAnonymityData(
    const blink::InterestGroupKey& interest_group_key,
    const InterestGroupKanonUpdateParameter& k_anon_data) {
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
  const std::vector<std::string>& ids_to_query =
      replace_existing_values ? k_anon_data.hashed_keys
                              : k_anon_data.newly_added_hashed_keys;
  if (ids_to_query.size() == 0) {
    return;
  }
  InProgressQueryState new_query_state(check_time, replace_existing_values);
  auto in_progress_query_it =
      queries_in_progress.insert({interest_group_key, new_query_state});
  if (!in_progress_query_it.second) {
    if (replace_existing_values ||
        in_progress_query_it.first->second.update_time <
            check_time - min_wait) {
      // This new request replaces all existing requests or the outstanding
      // request is outdated.
      in_progress_query_it.first->second = new_query_state;
    }
  }
  InProgressQueryState& query_state = in_progress_query_it.first->second;

  std::vector<std::string> ids_to_query_in_next_batch;
  for (const std::string& key : ids_to_query) {
    ids_to_query_in_next_batch.push_back(key);

    if (ids_to_query_in_next_batch.size() >= kQueryBatchSizeLimit) {
      query_state.remaining_responses++;
      k_anonymity_service->QuerySets(
          ids_to_query_in_next_batch,
          base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                         weak_ptr_factory_.GetWeakPtr(),
                         ids_to_query_in_next_batch, query_state.update_time,
                         interest_group_key));
      ids_to_query_in_next_batch.clear();
    }
  }

  if (ids_to_query_in_next_batch.empty()) {
    return;
  }
  query_state.remaining_responses++;
  k_anonymity_service->QuerySets(
      ids_to_query_in_next_batch,
      base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                     weak_ptr_factory_.GetWeakPtr(), ids_to_query_in_next_batch,
                     query_state.update_time, interest_group_key));
}

void InterestGroupKAnonymityManager::QuerySetsCallback(
    std::vector<std::string> hashed_query,
    base::Time update_time,
    const blink::InterestGroupKey& interest_group_key,
    std::vector<bool> status) {
  DCHECK_LE(status.size(), hashed_query.size());
  if (status.empty()) {
    // There was an error. Cancel the update.
    queries_in_progress.erase(interest_group_key);
    return;
  }

  auto it = queries_in_progress.find(interest_group_key);
  if (it == queries_in_progress.end() ||
      update_time != it->second.update_time) {
    return;
  }

  int size = std::min(hashed_query.size(), status.size());
  for (int i = 0; i < size; i++) {
    if (status[i]) {
      it->second.positive_hashed_keys_from_received_responses.emplace_back(
          hashed_query[i]);
    }
  }
  it->second.remaining_responses--;
  if (it->second.remaining_responses == 0) {
    interest_group_manager_->UpdateKAnonymity(
        interest_group_key,
        it->second.positive_hashed_keys_from_received_responses, update_time,
        it->second.replace_existing_values);
    queries_in_progress.erase(it);
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
