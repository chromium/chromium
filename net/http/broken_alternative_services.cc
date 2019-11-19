// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/broken_alternative_services.h"

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/http/http_server_properties.h"

namespace net {

namespace {

// Initial delay for broken alternative services.
const uint64_t kBrokenAlternativeProtocolDelaySecs = 300;
// Subsequent failures result in exponential (base 2) backoff.
// Limit binary shift to limit delay to approximately 2 days.
const int kBrokenDelayMaxShift = 9;

base::TimeDelta ComputeBrokenAlternativeServiceExpirationDelay(
    int broken_count) {
  DCHECK_GE(broken_count, 0);
  if (broken_count > kBrokenDelayMaxShift)
    broken_count = kBrokenDelayMaxShift;
  return base::TimeDelta::FromSeconds(kBrokenAlternativeProtocolDelaySecs) *
         (1 << broken_count);
}

}  // namespace

BrokenAlternativeService::BrokenAlternativeService(
    const AlternativeService& alternative_service,
    const NetworkIsolationKey& network_isolation_key,
    bool use_network_isolation_key)
    : alternative_service(alternative_service),
      network_isolation_key(use_network_isolation_key ? network_isolation_key
                                                      : NetworkIsolationKey()) {
}

BrokenAlternativeService::~BrokenAlternativeService() = default;

bool BrokenAlternativeService::operator<(
    const BrokenAlternativeService& other) const {
  return std::tie(alternative_service, network_isolation_key) <
         std::tie(other.alternative_service, other.network_isolation_key);
}

BrokenAlternativeServices::BrokenAlternativeServices(
    int max_recently_broken_alternative_service_entries,
    Delegate* delegate,
    const base::TickClock* clock)
    : delegate_(delegate),
      clock_(clock),
      recently_broken_alternative_services_(
          max_recently_broken_alternative_service_entries) {
  DCHECK(delegate_);
  DCHECK(clock_);
}

BrokenAlternativeServices::~BrokenAlternativeServices() = default;

void BrokenAlternativeServices::Clear() {
  expiration_timer_.Stop();
  broken_alternative_service_list_.clear();
  broken_alternative_service_map_.clear();
  recently_broken_alternative_services_.Clear();
}

void BrokenAlternativeServices::MarkBrokenUntilDefaultNetworkChanges(
    const BrokenAlternativeService& broken_alternative_service) {
  DCHECK(!broken_alternative_service.alternative_service.host.empty());
  DCHECK_NE(kProtoUnknown,
            broken_alternative_service.alternative_service.protocol);

  // The brokenness will expire on the default network change or based on
  // timer.
  broken_alternative_services_on_default_network_.insert(
      broken_alternative_service);
  MarkBrokenImpl(broken_alternative_service);
}

void BrokenAlternativeServices::MarkBroken(
    const BrokenAlternativeService& broken_alternative_service) {
  // The brokenness expires based only on the timer, not on the default network
  // change.
  broken_alternative_services_on_default_network_.erase(
      broken_alternative_service);
  MarkBrokenImpl(broken_alternative_service);
}

void BrokenAlternativeServices::MarkBrokenImpl(
    const BrokenAlternativeService& broken_alternative_service) {
  // Empty host means use host of origin, callers are supposed to substitute.
  DCHECK(!broken_alternative_service.alternative_service.host.empty());
  DCHECK_NE(kProtoUnknown,
            broken_alternative_service.alternative_service.protocol);

  auto it =
      recently_broken_alternative_services_.Get(broken_alternative_service);
  int broken_count = 0;
  if (it == recently_broken_alternative_services_.end()) {
    recently_broken_alternative_services_.Put(broken_alternative_service, 1);
  } else {
    broken_count = it->second++;
  }
  base::TimeTicks expiration =
      clock_->NowTicks() +
      ComputeBrokenAlternativeServiceExpirationDelay(broken_count);
  // Return if alternative service is already in expiration queue.
  BrokenAlternativeServiceList::iterator list_it;
  if (!AddToBrokenListAndMap(broken_alternative_service, expiration,
                             &list_it)) {
    return;
  }

  // If this is now the first entry in the list (i.e.
  // |broken_alternative_service| is the next alt svc to expire), schedule
  // an expiration task for it.
  if (list_it == broken_alternative_service_list_.begin()) {
    ScheduleBrokenAlternateProtocolMappingsExpiration();
  }
}

void BrokenAlternativeServices::MarkRecentlyBroken(
    const BrokenAlternativeService& broken_alternative_service) {
  DCHECK_NE(kProtoUnknown,
            broken_alternative_service.alternative_service.protocol);
  if (recently_broken_alternative_services_.Get(broken_alternative_service) ==
      recently_broken_alternative_services_.end()) {
    recently_broken_alternative_services_.Put(broken_alternative_service, 1);
  }
}

bool BrokenAlternativeServices::IsBroken(
    const BrokenAlternativeService& broken_alternative_service) const {
  // Empty host means use host of origin, callers are supposed to substitute.
  DCHECK(!broken_alternative_service.alternative_service.host.empty());
  return broken_alternative_service_map_.find(broken_alternative_service) !=
         broken_alternative_service_map_.end();
}

bool BrokenAlternativeServices::IsBroken(
    const BrokenAlternativeService& broken_alternative_service,
    base::TimeTicks* brokenness_expiration) const {
  DCHECK(brokenness_expiration != nullptr);
  // Empty host means use host of origin, callers are supposed to substitute.
  DCHECK(!broken_alternative_service.alternative_service.host.empty());
  auto map_it =
      broken_alternative_service_map_.find(broken_alternative_service);
  if (map_it == broken_alternative_service_map_.end()) {
    return false;
  }
  auto list_it = map_it->second;
  *brokenness_expiration = list_it->second;
  return true;
}

bool BrokenAlternativeServices::WasRecentlyBroken(
    const BrokenAlternativeService& broken_alternative_service) {
  DCHECK(!broken_alternative_service.alternative_service.host.empty());
  return recently_broken_alternative_services_.Get(
             broken_alternative_service) !=
             recently_broken_alternative_services_.end() ||
         broken_alternative_service_map_.find(broken_alternative_service) !=
             broken_alternative_service_map_.end();
}

void BrokenAlternativeServices::Confirm(
    const BrokenAlternativeService& broken_alternative_service) {
  DCHECK_NE(kProtoUnknown,
            broken_alternative_service.alternative_service.protocol);

  // Remove |broken_alternative_service| from
  // |broken_alternative_service_list_|, |broken_alternative_service_map_| and
  // |broken_alternative_services_on_default_network_|.
  auto map_it =
      broken_alternative_service_map_.find(broken_alternative_service);
  if (map_it != broken_alternative_service_map_.end()) {
    broken_alternative_service_list_.erase(map_it->second);
    broken_alternative_service_map_.erase(map_it);
  }

  auto it =
      recently_broken_alternative_services_.Get(broken_alternative_service);
  if (it != recently_broken_alternative_services_.end()) {
    recently_broken_alternative_services_.Erase(it);
  }

  broken_alternative_services_on_default_network_.erase(
      broken_alternative_service);
}

bool BrokenAlternativeServices::OnDefaultNetworkChanged() {
  bool changed = !broken_alternative_services_on_default_network_.empty();
  while (!broken_alternative_services_on_default_network_.empty()) {
    Confirm(*broken_alternative_services_on_default_network_.begin());
  }
  return changed;
}

void BrokenAlternativeServices::SetBrokenAndRecentlyBrokenAlternativeServices(
    std::unique_ptr<BrokenAlternativeServiceList>
        broken_alternative_service_list,
    std::unique_ptr<RecentlyBrokenAlternativeServices>
        recently_broken_alternative_services) {
  DCHECK(broken_alternative_service_list);
  DCHECK(recently_broken_alternative_services);

  base::TimeTicks next_expiration =
      broken_alternative_service_list_.empty()
          ? base::TimeTicks::Max()
          : broken_alternative_service_list_.front().second;

  // Add |recently_broken_alternative_services| to
  // |recently_broken_alternative_services_|.
  // If an alt-svc already exists, overwrite its broken-count to the one in
  // |recently_broken_alternative_services|.

  recently_broken_alternative_services_.Swap(
      *recently_broken_alternative_services);
  // Add back all existing recently broken alt svcs to cache so they're at
  // front of recency list (MRUCache::Get() does this automatically).
  for (auto it = recently_broken_alternative_services->rbegin();
       it != recently_broken_alternative_services->rend(); ++it) {
    if (recently_broken_alternative_services_.Get(it->first) ==
        recently_broken_alternative_services_.end()) {
      recently_broken_alternative_services_.Put(it->first, it->second);
    }
  }

  // Append |broken_alternative_service_list| to
  // |broken_alternative_service_list_|
  size_t num_broken_alt_svcs_added = broken_alternative_service_list->size();
  broken_alternative_service_list_.splice(
      broken_alternative_service_list_.begin(),
      *broken_alternative_service_list);
  // For each newly-appended alt svc in |broken_alternative_service_list_|,
  // add an entry to |broken_alternative_service_map_| that points to its
  // list iterator. Also, add an entry for that alt svc in
  // |recently_broken_alternative_services_| if one doesn't exist.
  auto list_it = broken_alternative_service_list_.begin();
  for (size_t i = 0; i < num_broken_alt_svcs_added; ++i) {
    const BrokenAlternativeService& broken_alternative_service = list_it->first;
    auto map_it =
        broken_alternative_service_map_.find(broken_alternative_service);
    if (map_it != broken_alternative_service_map_.end()) {
      // Implies this entry already exists somewhere else in
      // |broken_alternative_service_list_|. Remove the existing entry from
      // |broken_alternative_service_list_|, and update the
      // |broken_alternative_service_map_| entry to point to this list entry
      // instead.
      auto list_existing_entry_it = map_it->second;
      broken_alternative_service_list_.erase(list_existing_entry_it);
      map_it->second = list_it;
    } else {
      broken_alternative_service_map_.insert(
          std::make_pair(broken_alternative_service, list_it));
    }

    if (recently_broken_alternative_services_.Peek(
            broken_alternative_service) ==
        recently_broken_alternative_services_.end()) {
      recently_broken_alternative_services_.Put(broken_alternative_service, 1);
    }

    ++list_it;
  }

  // Sort |broken_alternative_service_list_| by expiration time. This operation
  // does not invalidate list iterators, so |broken_alternative_service_map_|
  // does not need to be updated.
  broken_alternative_service_list_.sort(
      [](const std::pair<BrokenAlternativeService, base::TimeTicks>& lhs,
         const std::pair<BrokenAlternativeService, base::TimeTicks>& rhs)
          -> bool { return lhs.second < rhs.second; });

  base::TimeTicks new_next_expiration =
      broken_alternative_service_list_.empty()
          ? base::TimeTicks::Max()
          : broken_alternative_service_list_.front().second;

  if (new_next_expiration != next_expiration)
    ScheduleBrokenAlternateProtocolMappingsExpiration();
}

const BrokenAlternativeServiceList&
BrokenAlternativeServices::broken_alternative_service_list() const {
  return broken_alternative_service_list_;
}

const RecentlyBrokenAlternativeServices&
BrokenAlternativeServices::recently_broken_alternative_services() const {
  return recently_broken_alternative_services_;
}

bool BrokenAlternativeServices::AddToBrokenListAndMap(
    const BrokenAlternativeService& broken_alternative_service,
    base::TimeTicks expiration,
    BrokenAlternativeServiceList::iterator* it) {
  DCHECK(it);

  auto map_it =
      broken_alternative_service_map_.find(broken_alternative_service);
  if (map_it != broken_alternative_service_map_.end())
    return false;

  // Iterate from end of |broken_alternative_service_list_| to find where to
  // insert it to keep the list sorted by expiration time.
  auto list_it = broken_alternative_service_list_.end();
  while (list_it != broken_alternative_service_list_.begin()) {
    --list_it;
    if (list_it->second <= expiration) {
      ++list_it;
      break;
    }
  }

  // Insert |broken_alternative_service| into the list and the map.
  list_it = broken_alternative_service_list_.insert(
      list_it, std::make_pair(broken_alternative_service, expiration));
  broken_alternative_service_map_.insert(
      std::make_pair(broken_alternative_service, list_it));

  *it = list_it;
  return true;
}

void BrokenAlternativeServices::ExpireBrokenAlternateProtocolMappings() {
  base::TimeTicks now = clock_->NowTicks();

  while (!broken_alternative_service_list_.empty()) {
    auto it = broken_alternative_service_list_.begin();
    if (now < it->second) {
      break;
    }

    delegate_->OnExpireBrokenAlternativeService(
        it->first.alternative_service, it->first.network_isolation_key);

    broken_alternative_service_map_.erase(it->first);
    broken_alternative_service_list_.erase(it);
  }

  if (!broken_alternative_service_list_.empty())
    ScheduleBrokenAlternateProtocolMappingsExpiration();
}

void BrokenAlternativeServices ::
    ScheduleBrokenAlternateProtocolMappingsExpiration() {
  DCHECK(!broken_alternative_service_list_.empty());
  base::TimeTicks now = clock_->NowTicks();
  base::TimeTicks next_expiration =
      broken_alternative_service_list_.front().second;
  base::TimeDelta delay =
      next_expiration > now ? next_expiration - now : base::TimeDelta();
  expiration_timer_.Stop();
  expiration_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(
          &BrokenAlternativeServices ::ExpireBrokenAlternateProtocolMappings,
          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace net
