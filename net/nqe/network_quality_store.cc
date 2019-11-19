// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/network_change_notifier.h"

namespace net {

namespace nqe {

namespace internal {

NetworkQualityStore::NetworkQualityStore() {
  static_assert(kMaximumNetworkQualityCacheSize > 0,
                "Size of the network quality cache must be > 0");
  // This limit should not be increased unless the logic for removing the
  // oldest cache entry is rewritten to use a doubly-linked-list LRU queue.
  static_assert(kMaximumNetworkQualityCacheSize <= 20,
                "Size of the network quality cache must <= 20");
}

NetworkQualityStore::~NetworkQualityStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NetworkQualityStore::Add(
    const nqe::internal::NetworkID& network_id,
    const nqe::internal::CachedNetworkQuality& cached_network_quality) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(cached_network_qualities_.size(),
            static_cast<size_t>(kMaximumNetworkQualityCacheSize));

  if (cached_network_quality.effective_connection_type() ==
      EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    return;
  }

  // Remove the entry from the map, if it is already present.
  cached_network_qualities_.erase(network_id);

  if (cached_network_qualities_.size() == kMaximumNetworkQualityCacheSize) {
    // Remove the oldest entry.
    auto oldest_entry_iterator = cached_network_qualities_.begin();

    for (auto it = cached_network_qualities_.begin();
         it != cached_network_qualities_.end(); ++it) {
      if ((it->second).OlderThan(oldest_entry_iterator->second))
        oldest_entry_iterator = it;
    }
    cached_network_qualities_.erase(oldest_entry_iterator);
  }

  cached_network_qualities_.insert(
      std::make_pair(network_id, cached_network_quality));
  DCHECK_LE(cached_network_qualities_.size(),
            static_cast<size_t>(kMaximumNetworkQualityCacheSize));

  for (auto& observer : network_qualities_cache_observer_list_)
    observer.OnChangeInCachedNetworkQuality(network_id, cached_network_quality);
}

bool NetworkQualityStore::GetById(
    const nqe::internal::NetworkID& network_id,
    nqe::internal::CachedNetworkQuality* cached_network_quality) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // First check if an exact match can be found.
  for (auto it = cached_network_qualities_.begin();
       it != cached_network_qualities_.end(); ++it) {
    if (network_id.type != it->first.type || network_id.id != it->first.id) {
      // The |type| and |id| must match.
      continue;
    }

    // Check for an exact match, and return immediately if one is found.
    // It's possible that the current network does not have signal strength
    // available. In that case, return the cached network quality when the
    // signal strength was unavailable.
    if (network_id.signal_strength == it->first.signal_strength) {
      *cached_network_quality = it->second;
      return true;
    }
  }

  // Handle the case when current network does not have signal strength
  // available. Return the cached network quality that corresponds to the
  // highest signal strength. This ensures that the method returns the fastest
  // network quality possible for the current network, and serves as a
  // conservative estimate.
  if (network_id.signal_strength == INT32_MIN) {
    auto matching_it = cached_network_qualities_.end();

    for (auto it = cached_network_qualities_.begin();
         it != cached_network_qualities_.end(); ++it) {
      if (network_id.type != it->first.type || network_id.id != it->first.id) {
        // The |type| and |id| must match.
        continue;
      }

      // The cached network must have signal strength available. If the cached
      // signal strength is unavailable, then this case would have been handled
      // above.
      DCHECK_NE(INT32_MIN, it->first.signal_strength);

      if (matching_it == cached_network_qualities_.end() ||
          it->first.signal_strength > matching_it->first.signal_strength) {
        matching_it = it;
      }
    }

    if (matching_it == cached_network_qualities_.end())
      return false;

    *cached_network_quality = matching_it->second;
    return true;
  }

  // Finally, handle the case where the current network has a valid signal
  // strength, but there is no exact match.

  // |matching_it| points to the entry that has the same connection type and
  // id as |network_id|, and has the signal strength closest to the signal
  // stength of |network_id|.
  auto matching_it = cached_network_qualities_.end();
  int matching_it_diff_signal_strength = INT32_MAX;

  // Find the closest estimate.
  for (auto it = cached_network_qualities_.begin();
       it != cached_network_qualities_.end(); ++it) {
    if (network_id.type != it->first.type || network_id.id != it->first.id) {
      // The |type| and |id| must match.
      continue;
    }

    DCHECK_LE(0, network_id.signal_strength);

    // Determine if the signal strength of |network_id| is closer to the
    // signal strength of the network at |it| then that of the network at
    // |matching_it|.
    int diff_signal_strength =
        std::abs(network_id.signal_strength - it->first.signal_strength);
    if (it->first.signal_strength == INT32_MIN) {
      // Current network has signal strength available. However, the persisted
      // network does not. Set the |diff_signal_strength| to INT32_MAX. This
      // ensures that if an entry with a valid signal strength is found later
      // during iteration, then that entry will be used. If no entry with valid
      // signal strength is found, then this entry will be used.
      diff_signal_strength = INT32_MAX;
    }

    if (matching_it == cached_network_qualities_.end() ||
        diff_signal_strength < matching_it_diff_signal_strength) {
      matching_it = it;
      matching_it_diff_signal_strength = diff_signal_strength;
    }
  }

  if (matching_it == cached_network_qualities_.end())
    return false;

  *cached_network_quality = matching_it->second;
  return true;
}

void NetworkQualityStore::AddNetworkQualitiesCacheObserver(
    NetworkQualitiesCacheObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_qualities_cache_observer_list_.AddObserver(observer);

  // Notify the |observer| on the next message pump since |observer| may not
  // be completely set up for receiving the callbacks.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&NetworkQualityStore::NotifyCacheObserverIfPresent,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

void NetworkQualityStore::RemoveNetworkQualitiesCacheObserver(
    NetworkQualitiesCacheObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_qualities_cache_observer_list_.RemoveObserver(observer);
}

void NetworkQualityStore::NotifyCacheObserverIfPresent(
    NetworkQualitiesCacheObserver* observer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_qualities_cache_observer_list_.HasObserver(observer))
    return;
  for (const auto it : cached_network_qualities_)
    observer->OnChangeInCachedNetworkQuality(it.first, it.second);
}

}  // namespace internal

}  // namespace nqe

}  // namespace net
