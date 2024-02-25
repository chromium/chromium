// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_qualities_prefs_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "net/nqe/network_quality_estimator.h"

namespace net {

namespace {

// Maximum size of the prefs that hold the qualities of different networks.
// A single entry in the cache consists of three tuples:
// (i)   SSID or MCCMNC of the network. SSID is at most 32 characters in length
//       (but is typically shorter than that). MCCMNC is at most 6 characters
//       long.
// (ii)  Connection type of the network as reported by network
//       change notifier (an enum).
// (iii) Effective connection type of the network (an enum).
constexpr size_t kMaxCacheSize = 20u;

// Parses |value| into a map of NetworkIDs and CachedNetworkQualities,
// and returns the map.
ParsedPrefs ConvertDictionaryValueToMap(const base::Value::Dict& value) {
  DCHECK_GE(kMaxCacheSize, value.size());

  ParsedPrefs read_prefs;
  for (auto it : value) {
    nqe::internal::NetworkID network_id =
        nqe::internal::NetworkID::FromString(it.first);

    if (!it.second.is_string())
      continue;
    std::optional<EffectiveConnectionType> effective_connection_type =
        GetEffectiveConnectionTypeForName(it.second.GetString());
    DCHECK(effective_connection_type.has_value());

    nqe::internal::CachedNetworkQuality cached_network_quality(
        effective_connection_type.value_or(EFFECTIVE_CONNECTION_TYPE_UNKNOWN));
    read_prefs[network_id] = cached_network_quality;
  }
  return read_prefs;
}

}  // namespace

NetworkQualitiesPrefsManager::NetworkQualitiesPrefsManager(
    std::unique_ptr<PrefDelegate> pref_delegate)
    : pref_delegate_(std::move(pref_delegate)),
      prefs_(pref_delegate_->GetDictionaryValue()) {
  DCHECK(pref_delegate_);
  DCHECK_GE(kMaxCacheSize, prefs_.size());
}

NetworkQualitiesPrefsManager::~NetworkQualitiesPrefsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ShutdownOnPrefSequence();

  if (network_quality_estimator_)
    network_quality_estimator_->RemoveNetworkQualitiesCacheObserver(this);
}

void NetworkQualitiesPrefsManager::InitializeOnNetworkThread(
    NetworkQualityEstimator* network_quality_estimator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_quality_estimator);

  // Read |prefs_| again since they have now been fully initialized. This
  // overwrites any values that may have been added to |prefs_| since
  // construction of |this| via OnChangeInCachedNetworkQuality(). However, it's
  // expected that InitializeOnNetworkThread will be called soon after
  // construction of |this|. So, any loss of values would be minimal.
  prefs_ = pref_delegate_->GetDictionaryValue();
  read_prefs_startup_ = ConvertDictionaryValueToMap(prefs_);

  network_quality_estimator_ = network_quality_estimator;
  network_quality_estimator_->AddNetworkQualitiesCacheObserver(this);

  // Notify network quality estimator of the read prefs.
  network_quality_estimator_->OnPrefsRead(read_prefs_startup_);
}

void NetworkQualitiesPrefsManager::ShutdownOnPrefSequence() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_delegate_.reset();
}

void NetworkQualitiesPrefsManager::ClearPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOCAL_HISTOGRAM_COUNTS_100("NQE.PrefsSizeOnClearing", prefs_.size());
  prefs_.clear();
  DCHECK_EQ(0u, prefs_.size());
  pref_delegate_->SetDictionaryValue(prefs_);
}

void NetworkQualitiesPrefsManager::OnChangeInCachedNetworkQuality(
    const nqe::internal::NetworkID& network_id,
    const nqe::internal::CachedNetworkQuality& cached_network_quality) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GE(kMaxCacheSize, prefs_.size());

  std::string network_id_string = network_id.ToString();

  // If the network ID contains a period, then return early since the dictionary
  // prefs cannot contain period in the path.
  if (network_id_string.find('.') != std::string::npos)
    return;

  prefs_.Set(network_id_string,
             GetNameForEffectiveConnectionType(
                 cached_network_quality.effective_connection_type()));

  if (prefs_.size() > kMaxCacheSize) {
    // Delete one randomly selected value that has a key that is different from
    // |network_id|.
    DCHECK_EQ(kMaxCacheSize + 1, prefs_.size());
    // Generate a random number in the range [0, |kMaxCacheSize| - 1] since the
    // number of network IDs in |prefs_| other than |network_id| is
    // |kMaxCacheSize|.
    int index_to_delete = base::RandInt(0, kMaxCacheSize - 1);

    for (auto it : prefs_) {
      // Delete the kth element in the dictionary, not including the element
      // that represents the current network. k == |index_to_delete|.
      if (nqe::internal::NetworkID::FromString(it.first) == network_id)
        continue;

      if (index_to_delete == 0) {
        prefs_.Remove(it.first);
        break;
      }
      index_to_delete--;
    }
  }
  DCHECK_GE(kMaxCacheSize, prefs_.size());

  // Notify the pref delegate so that it updates the prefs on the disk.
  pref_delegate_->SetDictionaryValue(prefs_);
}

ParsedPrefs NetworkQualitiesPrefsManager::ForceReadPrefsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict value = pref_delegate_->GetDictionaryValue();
  return ConvertDictionaryValueToMap(value);
}

}  // namespace net
