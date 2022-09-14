// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "crypto/sha2.h"

namespace content {
namespace {

constexpr base::TimeDelta kUpdateExpiration = base::Hours(24);

// Calculates a SHA256 hash of the input string.
std::string KAnonHash(const std::string& input) {
  return crypto::SHA256HashString(input);
}

}  // namespace

std::string KAnonKeyFor(const url::Origin& owner, const std::string& name) {
  return owner.GetURL().spec() + '\n' + name;
}

InterestGroupKAnonymityManager::InterestGroupKAnonymityManager(
    InterestGroupManagerImpl* interest_group_manager,
    std::unique_ptr<KAnonymityServiceDelegate> k_anonymity_service)
    : interest_group_manager_(interest_group_manager),
      k_anonymity_service_(std::move(k_anonymity_service)),
      weak_ptr_factory_(this) {}

InterestGroupKAnonymityManager::~InterestGroupKAnonymityManager() = default;

void InterestGroupKAnonymityManager::QueryKAnonymityForInterestGroup(
    const StorageInterestGroup& storage_group) {
  if (!k_anonymity_service_)
    return;

  std::vector<std::string> unhashed_ids_to_query;
  base::Time check_time = base::Time::Now();

  if (!storage_group.name_kanon ||
      storage_group.name_kanon->last_updated < check_time - kUpdateExpiration) {
    unhashed_ids_to_query.push_back(KAnonKeyFor(
        storage_group.interest_group.owner, storage_group.interest_group.name));
  }

  if (storage_group.interest_group.daily_update_url) {
    if (!storage_group.daily_update_url_kanon ||
        storage_group.daily_update_url_kanon->last_updated <
            check_time - kUpdateExpiration) {
      unhashed_ids_to_query.push_back(
          storage_group.interest_group.daily_update_url->spec());
    }
  }

  for (const auto& ad : storage_group.ads_kanon) {
    if (ad.last_updated < check_time - kUpdateExpiration) {
      unhashed_ids_to_query.push_back(ad.key);
    }
  }

  std::vector<std::string> hashed_ids_to_query;
  for (const auto& input : unhashed_ids_to_query) {
    hashed_ids_to_query.push_back(KAnonHash(input));
  }

  k_anonymity_service_->QuerySets(
      std::move(hashed_ids_to_query),
      base::BindOnce(&InterestGroupKAnonymityManager::QuerySetsCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(unhashed_ids_to_query), std::move(check_time)));
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
  for (size_t i = size; i < unhashed_query.size(); i++) {
    // If we fail, update the data set anyway until we can verify that the
    // server is stable.
    StorageInterestGroup::KAnonymityData data = {unhashed_query[i], false,
                                                 update_time};
    interest_group_manager_->UpdateKAnonymity(data);
  }
}

void InterestGroupKAnonymityManager::RegisterInterestGroupAsJoined(
    const blink::InterestGroup& group) {
  RegisterIDAsJoined(KAnonKeyFor(group.owner, group.name));
  if (group.daily_update_url)
    RegisterIDAsJoined(group.daily_update_url->spec());
}

void InterestGroupKAnonymityManager::RegisterAdAsWon(const GURL& render_url) {
  RegisterIDAsJoined(render_url.spec());
}

void InterestGroupKAnonymityManager::RegisterIDAsJoined(
    const std::string& key) {
  if (!k_anonymity_service_)
    return;
  interest_group_manager_->GetLastKAnonymityReported(
      key,
      base::BindOnce(&InterestGroupKAnonymityManager::OnGotLastReportedTime,
                     weak_ptr_factory_.GetWeakPtr(), key));
}

void InterestGroupKAnonymityManager::OnGotLastReportedTime(
    std::string key,
    absl::optional<base::Time> last_update_time) {
  DCHECK(last_update_time);
  if (!last_update_time)
    return;

  // If it has been long enough since we last joined
  if (base::Time::Now() <
      last_update_time.value_or(base::Time()) + kUpdateExpiration)
    return;

  std::string hash = KAnonHash(key);
  k_anonymity_service_->JoinSet(
      std::move(hash),
      base::BindOnce(&InterestGroupKAnonymityManager::JoinSetCallback,
                     weak_ptr_factory_.GetWeakPtr(), std::move(key)));
}

void InterestGroupKAnonymityManager::JoinSetCallback(std::string key,
                                                     bool status) {
  // Update the time regardless of status until we verify the server is stable.
  interest_group_manager_->UpdateLastKAnonymityReported(key);
}

}  // namespace content
