// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_filter.h"

#include <stdint.h>
#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "services/preferences/tracked/dictionary_hash_store_contents.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "services/preferences/tracked/tracked_atomic_preference.h"
#include "services/preferences/tracked/tracked_split_preference.h"

namespace {

void CleanupDeprecatedTrackedPreferences(
    base::DictionaryValue* pref_store_contents,
    PrefHashStoreTransaction* hash_store_transaction) {
  // Add deprecated previously tracked preferences below for them to be cleaned
  // up from both the pref files and the hash store.
  static const char* const kDeprecatedTrackedPreferences[] = {
      // TODO(a-v-y): Remove in M60+,
      "default_search_provider.search_url", "default_search_provider.name",
      "default_search_provider.keyword"};

  for (size_t i = 0; i < base::size(kDeprecatedTrackedPreferences); ++i) {
    const char* key = kDeprecatedTrackedPreferences[i];
    pref_store_contents->Remove(key, NULL);
    hash_store_transaction->ClearHash(key);
  }
}

}  // namespace

using PrefTrackingStrategy =
    prefs::mojom::TrackedPreferenceMetadata::PrefTrackingStrategy;

PrefHashFilter::PrefHashFilter(
    std::unique_ptr<PrefHashStore> pref_hash_store,
    StoreContentsPair external_validation_hash_store_pair,
    const std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>&
        tracked_preferences,
    mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
        reset_on_load_observer,
    prefs::mojom::TrackedPreferenceValidationDelegate* delegate,
    size_t reporting_ids_count)
    : pref_hash_store_(std::move(pref_hash_store)),
      external_validation_hash_store_pair_(
          external_validation_hash_store_pair.first
              ? base::make_optional(
                    std::move(external_validation_hash_store_pair))
              : base::nullopt),
      reset_on_load_observer_(std::move(reset_on_load_observer)) {
  DCHECK(pref_hash_store_);
  DCHECK_GE(reporting_ids_count, tracked_preferences.size());
  // Verify that, if |external_validation_hash_store_pair_| is present, both its
  // items are non-null.
  DCHECK(!external_validation_hash_store_pair_.has_value() ||
         (external_validation_hash_store_pair_->first &&
          external_validation_hash_store_pair_->second));

  for (size_t i = 0; i < tracked_preferences.size(); ++i) {
    const prefs::mojom::TrackedPreferenceMetadata& metadata =
        *tracked_preferences[i];

    std::unique_ptr<TrackedPreference> tracked_preference;
    switch (metadata.strategy) {
      case PrefTrackingStrategy::ATOMIC:
        tracked_preference.reset(new TrackedAtomicPreference(
            metadata.name, metadata.reporting_id, reporting_ids_count,
            metadata.enforcement_level, metadata.value_type, delegate));
        break;
      case PrefTrackingStrategy::SPLIT:
        tracked_preference.reset(new TrackedSplitPreference(
            metadata.name, metadata.reporting_id, reporting_ids_count,
            metadata.enforcement_level, metadata.value_type, delegate));
        break;
    }
    DCHECK(tracked_preference);

    bool is_new = tracked_paths_
                      .insert(std::make_pair(metadata.name,
                                             std::move(tracked_preference)))
                      .second;
    DCHECK(is_new);
  }
}

PrefHashFilter::~PrefHashFilter() {
  // Ensure new values for all |changed_paths_| have been flushed to
  // |pref_hash_store_| already.
  DCHECK(changed_paths_.empty());
}

// static
void PrefHashFilter::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // See GetResetTime for why this is a StringPref and not Int64Pref.
  registry->RegisterStringPref(
      user_prefs::kPreferenceResetTime,
      base::NumberToString(base::Time().ToInternalValue()));
}

// static
base::Time PrefHashFilter::GetResetTime(PrefService* user_prefs) {
  // Provide our own implementation (identical to the PrefService::GetInt64) in
  // order to ensure it remains consistent with the way we store this value
  // (which we do via a PrefStore, preventing us from reusing
  // PrefService::SetInt64).
  int64_t internal_value = base::Time().ToInternalValue();
  if (!base::StringToInt64(
          user_prefs->GetString(user_prefs::kPreferenceResetTime),
          &internal_value)) {
    // Somehow the value stored on disk is not a valid int64_t.
    NOTREACHED();
    return base::Time();
  }
  return base::Time::FromInternalValue(internal_value);
}

// static
void PrefHashFilter::ClearResetTime(PrefService* user_prefs) {
  user_prefs->ClearPref(user_prefs::kPreferenceResetTime);
}

void PrefHashFilter::Initialize(base::DictionaryValue* pref_store_contents) {
  DictionaryHashStoreContents dictionary_contents(pref_store_contents);
  std::unique_ptr<PrefHashStoreTransaction> hash_store_transaction(
      pref_hash_store_->BeginTransaction(&dictionary_contents));
  for (auto it = tracked_paths_.begin(); it != tracked_paths_.end(); ++it) {
    const std::string& initialized_path = it->first;
    const TrackedPreference* initialized_preference = it->second.get();
    const base::Value* value = nullptr;
    pref_store_contents->Get(initialized_path, &value);
    initialized_preference->OnNewValue(value, hash_store_transaction.get());
  }
}

// Marks |path| has having changed if it is part of |tracked_paths_|. A new hash
// will be stored for it the next time FilterSerializeData() is invoked.
void PrefHashFilter::FilterUpdate(const std::string& path) {
  auto it = tracked_paths_.find(path);
  if (it != tracked_paths_.end())
    changed_paths_.insert(std::make_pair(path, it->second.get()));
}

// Updates the stored hashes for |changed_paths_| before serializing data to
// disk. This is required as storing the hash everytime a pref's value changes
// is too expensive (see perf regression @ http://crbug.com/331273).
PrefFilter::OnWriteCallbackPair PrefHashFilter::FilterSerializeData(
    base::DictionaryValue* pref_store_contents) {
  // Generate the callback pair before clearing |changed_paths_|.
  PrefFilter::OnWriteCallbackPair callback_pair =
      GetOnWriteSynchronousCallbacks(pref_store_contents);

  if (!changed_paths_.empty()) {
    base::TimeTicks checkpoint = base::TimeTicks::Now();
    {
      DictionaryHashStoreContents dictionary_contents(pref_store_contents);
      std::unique_ptr<PrefHashStoreTransaction> hash_store_transaction(
          pref_hash_store_->BeginTransaction(&dictionary_contents));

      std::unique_ptr<PrefHashStoreTransaction>
          external_validation_hash_store_transaction;
      if (external_validation_hash_store_pair_) {
        external_validation_hash_store_transaction =
            external_validation_hash_store_pair_->first->BeginTransaction(
                external_validation_hash_store_pair_->second.get());
      }

      for (ChangedPathsMap::const_iterator it = changed_paths_.begin();
           it != changed_paths_.end(); ++it) {
        const std::string& changed_path = it->first;
        const TrackedPreference* changed_preference = it->second;
        const base::Value* value = nullptr;
        pref_store_contents->Get(changed_path, &value);
        changed_preference->OnNewValue(value, hash_store_transaction.get());
      }
      changed_paths_.clear();
    }
    UMA_HISTOGRAM_TIMES("Settings.FilterSerializeDataTime",
                        base::TimeTicks::Now() - checkpoint);
  }

  return callback_pair;
}

void PrefHashFilter::OnStoreDeletionFromDisk() {
  if (external_validation_hash_store_pair_) {
    external_validation_hash_store_pair_->second.get()->Reset();

    // The PrefStore will attempt to write preferences even if it's marked for
    // deletion. Clear the external store pair to avoid re-writing to the
    // external store.
    external_validation_hash_store_pair_.reset();
  }
}

void PrefHashFilter::FinalizeFilterOnLoad(
    PostFilterOnLoadCallback post_filter_on_load_callback,
    std::unique_ptr<base::DictionaryValue> pref_store_contents,
    bool prefs_altered) {
  DCHECK(pref_store_contents);
  base::TimeTicks checkpoint = base::TimeTicks::Now();

  bool did_reset = false;
  {
    DictionaryHashStoreContents dictionary_contents(pref_store_contents.get());
    std::unique_ptr<PrefHashStoreTransaction> hash_store_transaction(
        pref_hash_store_->BeginTransaction(&dictionary_contents));

    std::unique_ptr<PrefHashStoreTransaction>
        external_validation_hash_store_transaction;
    if (external_validation_hash_store_pair_) {
      external_validation_hash_store_transaction =
          external_validation_hash_store_pair_->first->BeginTransaction(
              external_validation_hash_store_pair_->second.get());
    }

    CleanupDeprecatedTrackedPreferences(pref_store_contents.get(),
                                        hash_store_transaction.get());

    for (auto it = tracked_paths_.begin(); it != tracked_paths_.end(); ++it) {
      if (it->second->EnforceAndReport(
              pref_store_contents.get(), hash_store_transaction.get(),
              external_validation_hash_store_transaction.get())) {
        did_reset = true;
        prefs_altered = true;
      }
    }
    if (hash_store_transaction->StampSuperMac())
      prefs_altered = true;
  }

  if (did_reset) {
    pref_store_contents->SetString(
        user_prefs::kPreferenceResetTime,
        base::NumberToString(base::Time::Now().ToInternalValue()));
    FilterUpdate(user_prefs::kPreferenceResetTime);

    if (reset_on_load_observer_)
      reset_on_load_observer_->OnResetOnLoad();
  }
  reset_on_load_observer_.reset();

  UMA_HISTOGRAM_TIMES("Settings.FilterOnLoadTime",
                      base::TimeTicks::Now() - checkpoint);

  std::move(post_filter_on_load_callback)
      .Run(std::move(pref_store_contents), prefs_altered);
}

// static
void PrefHashFilter::ClearFromExternalStore(
    HashStoreContents* external_validation_hash_store_contents,
    const base::DictionaryValue* changed_paths_and_macs) {
  DCHECK(!changed_paths_and_macs->empty());

  for (base::DictionaryValue::Iterator it(*changed_paths_and_macs);
       !it.IsAtEnd(); it.Advance()) {
    external_validation_hash_store_contents->RemoveEntry(it.key());
  }
}

// static
void PrefHashFilter::FlushToExternalStore(
    std::unique_ptr<HashStoreContents> external_validation_hash_store_contents,
    std::unique_ptr<base::DictionaryValue> changed_paths_and_macs,
    bool write_success) {
  DCHECK(!changed_paths_and_macs->empty());
  DCHECK(external_validation_hash_store_contents);
  if (!write_success)
    return;

  for (base::DictionaryValue::Iterator it(*changed_paths_and_macs);
       !it.IsAtEnd(); it.Advance()) {
    const std::string& changed_path = it.key();

    const base::DictionaryValue* split_values = nullptr;
    if (it.value().GetAsDictionary(&split_values)) {
      for (base::DictionaryValue::Iterator inner_it(*split_values);
           !inner_it.IsAtEnd(); inner_it.Advance()) {
        std::string mac;
        bool is_string = inner_it.value().GetAsString(&mac);
        DCHECK(is_string);

        external_validation_hash_store_contents->SetSplitMac(
            changed_path, inner_it.key(), mac);
      }
    } else {
      const base::Value* value_as_string;
      bool is_string = it.value().GetAsString(&value_as_string);
      DCHECK(is_string);

      external_validation_hash_store_contents->SetMac(
          changed_path, value_as_string->GetString());
    }
  }
}

PrefFilter::OnWriteCallbackPair PrefHashFilter::GetOnWriteSynchronousCallbacks(
    base::DictionaryValue* pref_store_contents) {
  if (changed_paths_.empty() || !external_validation_hash_store_pair_) {
    return std::make_pair(base::OnceClosure(),
                          base::OnceCallback<void(bool success)>());
  }

  std::unique_ptr<base::DictionaryValue> changed_paths_macs =
      std::make_unique<base::DictionaryValue>();

  for (ChangedPathsMap::const_iterator it = changed_paths_.begin();
       it != changed_paths_.end(); ++it) {
    const std::string& changed_path = it->first;
    const TrackedPreference* changed_preference = it->second;

    switch (changed_preference->GetType()) {
      case TrackedPreferenceType::ATOMIC: {
        const base::Value* new_value = nullptr;
        pref_store_contents->Get(changed_path, &new_value);
        changed_paths_macs->SetKey(
            changed_path,
            base::Value(external_validation_hash_store_pair_->first->ComputeMac(
                changed_path, new_value)));
        break;
      }
      case TrackedPreferenceType::SPLIT: {
        const base::DictionaryValue* dict_value = nullptr;
        pref_store_contents->GetDictionary(changed_path, &dict_value);
        changed_paths_macs->SetWithoutPathExpansion(
            changed_path,
            external_validation_hash_store_pair_->first->ComputeSplitMacs(
                changed_path, dict_value));
        break;
      }
    }
  }

  DCHECK(external_validation_hash_store_pair_->second->IsCopyable())
      << "External HashStoreContents must be copyable as it needs to be used "
         "off-thread";

  std::unique_ptr<HashStoreContents> hash_store_contents_copy =
      external_validation_hash_store_pair_->second->MakeCopy();

  // We can use raw pointers for the first callback instead of making more
  // copies as it will be executed in sequence before the second callback,
  // which owns the pointers.
  HashStoreContents* raw_contents = hash_store_contents_copy.get();
  base::DictionaryValue* raw_changed_paths_macs = changed_paths_macs.get();

  return std::make_pair(
      base::BindOnce(&ClearFromExternalStore, base::Unretained(raw_contents),
                     base::Unretained(raw_changed_paths_macs)),
      base::BindOnce(&FlushToExternalStore, std::move(hash_store_contents_copy),
                     std::move(changed_paths_macs)));
}
