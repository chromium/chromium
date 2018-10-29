// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_qualities_prefs_manager.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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
ParsedPrefs ConvertDictionaryValueToMap(const base::DictionaryValue* value) {
  DCHECK_GE(kMaxCacheSize, value->size());

  ParsedPrefs read_prefs;
  for (const auto& it : value->DictItems()) {
    nqe::internal::NetworkID network_id =
        nqe::internal::NetworkID::FromString(it.first);

    std::string effective_connection_type_string;
    const bool effective_connection_type_available =
        it.second.GetAsString(&effective_connection_type_string);
    DCHECK(effective_connection_type_available);

    base::Optional<EffectiveConnectionType> effective_connection_type =
        GetEffectiveConnectionTypeForName(effective_connection_type_string);
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
      pref_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      prefs_(pref_delegate_->GetDictionaryValue()),
      network_quality_estimator_(nullptr),
      read_prefs_startup_(ConvertDictionaryValueToMap(prefs_.get())),
      pref_weak_ptr_factory_(this) {
  DCHECK(pref_delegate_);
  DCHECK(pref_task_runner_);
  DCHECK_GE(kMaxCacheSize, prefs_->size());

  pref_weak_ptr_ = pref_weak_ptr_factory_.GetWeakPtr();
}

NetworkQualitiesPrefsManager::~NetworkQualitiesPrefsManager() {
  if (!network_task_runner_)
    return;
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
  if (network_quality_estimator_)
    network_quality_estimator_->RemoveNetworkQualitiesCacheObserver(this);
}

void NetworkQualitiesPrefsManager::InitializeOnNetworkThread(
    NetworkQualityEstimator* network_quality_estimator) {
  DCHECK(!network_task_runner_);
  DCHECK(network_quality_estimator);

  network_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  network_quality_estimator_ = network_quality_estimator;
  network_quality_estimator_->AddNetworkQualitiesCacheObserver(this);

  // Notify network quality estimator of the read prefs.
  network_quality_estimator_->OnPrefsRead(read_prefs_startup_);
}

void NetworkQualitiesPrefsManager::OnChangeInCachedNetworkQuality(
    const nqe::internal::NetworkID& network_id,
    const nqe::internal::CachedNetworkQuality& cached_network_quality) {
  DCHECK(network_task_runner_->RunsTasksInCurrentSequence());

  // Notify |this| on the pref thread.
  pref_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NetworkQualitiesPrefsManager::
                     OnChangeInCachedNetworkQualityOnPrefSequence,
                 pref_weak_ptr_, network_id, cached_network_quality));
}

void NetworkQualitiesPrefsManager::ShutdownOnPrefSequence() {
  DCHECK(pref_task_runner_->RunsTasksInCurrentSequence());
  pref_weak_ptr_factory_.InvalidateWeakPtrs();
  pref_delegate_.reset();
}

void NetworkQualitiesPrefsManager::ClearPrefs() {
  DCHECK(pref_task_runner_->RunsTasksInCurrentSequence());
  prefs_->Clear();
  DCHECK_EQ(0u, prefs_->size());
  pref_delegate_->SetDictionaryValue(*prefs_);
}

void NetworkQualitiesPrefsManager::OnChangeInCachedNetworkQualityOnPrefSequence(
    const nqe::internal::NetworkID& network_id,
    const nqe::internal::CachedNetworkQuality& cached_network_quality) {
  // The prefs can only be written on the pref thread.
  DCHECK(pref_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_GE(kMaxCacheSize, prefs_->size());

  std::string network_id_string = network_id.ToString();

  // If the network ID contains a period, then return early since the dictionary
  // prefs cannot contain period in the path.
  if (network_id_string.find('.') != std::string::npos)
    return;

  prefs_->SetString(network_id_string,
                    GetNameForEffectiveConnectionType(
                        cached_network_quality.effective_connection_type()));

  if (prefs_->size() > kMaxCacheSize) {
    // Delete one randomly selected value that has a key that is different from
    // |network_id|.
    DCHECK_EQ(kMaxCacheSize + 1, prefs_->size());
    // Generate a random number in the range [0, |kMaxCacheSize| - 1] since the
    // number of network IDs in |prefs_| other than |network_id| is
    // |kMaxCacheSize|.
    int index_to_delete = base::RandInt(0, kMaxCacheSize - 1);

    for (const auto& it : prefs_->DictItems()) {
      // Delete the kth element in the dictionary, not including the element
      // that represents the current network. k == |index_to_delete|.
      if (nqe::internal::NetworkID::FromString(it.first) == network_id)
        continue;

      if (index_to_delete == 0) {
        prefs_->RemoveKey(it.first);
        break;
      }
      index_to_delete--;
    }
  }
  DCHECK_GE(kMaxCacheSize, prefs_->size());

  // Notify the pref delegate so that it updates the prefs on the disk.
  pref_delegate_->SetDictionaryValue(*prefs_);
}

ParsedPrefs NetworkQualitiesPrefsManager::ForceReadPrefsForTesting() const {
  DCHECK(pref_task_runner_->RunsTasksInCurrentSequence());
  std::unique_ptr<base::DictionaryValue> value(
      pref_delegate_->GetDictionaryValue());
  return ConvertDictionaryValueToMap(value.get());
}

}  // namespace net
