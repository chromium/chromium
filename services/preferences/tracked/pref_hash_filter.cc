// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_filter.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
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

std::vector<const char*>* GetDeprecatedPrefs() {
  // Add deprecated previously tracked preferences below for them to be cleaned
  // up from both the pref files and the hash store.
  static std::vector<const char*> prefs{
#if BUILDFLAG(IS_WIN)
      // TODO(crbug.com/40265803): Remove after Oct 2024
      "software_reporter.prompt_version",
      "software_reporter.prompt_seed",
      "settings_reset_prompt.prompt_wave",
      "settings_reset_prompt.last_triggered_for_default_search",
      "settings_reset_prompt.last_triggered_for_startup_urls",
      "settings_reset_prompt.last_triggered_for_homepage",
      "software_reporter.reporting",
      // Also delete the now empty dictionaries.
      "software_reporter",
      "settings_reset_prompt",
      // Added Aug'24. Remove after Aug'25.
      "google.services.last_account_id",
#endif
  };

  return &prefs;
}

void CleanupDeprecatedTrackedPreferences(
    base::Value::Dict& pref_store_contents,
    PrefHashStoreTransaction* hash_store_transaction) {
  for (const char* key : *GetDeprecatedPrefs()) {
    pref_store_contents.RemoveByDottedPath(key);
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
    scoped_refptr<base::RefCountedData<
        mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>
        delegate,
    size_t reporting_ids_count)
    : pref_hash_store_(std::move(pref_hash_store)),
      external_validation_hash_store_pair_(
          external_validation_hash_store_pair.first
              ? std::make_optional(
                    std::move(external_validation_hash_store_pair))
              : std::nullopt),
      reset_on_load_observer_(std::move(reset_on_load_observer)),
      delegate_(std::move(delegate)) {
  DCHECK(pref_hash_store_);
  DCHECK_GE(reporting_ids_count, tracked_preferences.size());
  // Verify that, if |external_validation_hash_store_pair_| is present, both its
  // items are non-null.
  DCHECK(!external_validation_hash_store_pair_.has_value() ||
         (external_validation_hash_store_pair_->first &&
          external_validation_hash_store_pair_->second));

  prefs::mojom::TrackedPreferenceValidationDelegate* delegate_ptr =
      (delegate_ ? delegate_->data.get() : nullptr);
  for (size_t i = 0; i < tracked_preferences.size(); ++i) {
    const prefs::mojom::TrackedPreferenceMetadata& metadata =
        *tracked_preferences[i];

    std::unique_ptr<TrackedPreference> tracked_preference;
    switch (metadata.strategy) {
      case PrefTrackingStrategy::ATOMIC:
        tracked_preference = std::make_unique<TrackedAtomicPreference>(
            metadata.name, metadata.reporting_id, reporting_ids_count,
            metadata.enforcement_level, metadata.value_type, delegate_ptr);
        break;
      case PrefTrackingStrategy::SPLIT:
        tracked_preference = std::make_unique<TrackedSplitPreference>(
            metadata.name, metadata.reporting_id, reporting_ids_count,
            metadata.enforcement_level, metadata.value_type, delegate_ptr);
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
    NOTREACHED_IN_MIGRATION();
    return base::Time();
  }
  return base::Time::FromInternalValue(internal_value);
}

// static
void PrefHashFilter::ClearResetTime(PrefService* user_prefs) {
  user_prefs->ClearPref(user_prefs::kPreferenceResetTime);
}

void PrefHashFilter::Initialize(base::Value::Dict& pref_store_contents) {
  DictionaryHashStoreContents dictionary_contents(pref_store_contents);
  std::unique_ptr<PrefHashStoreTransaction> hash_store_transaction(
      pref_hash_store_->BeginTransaction(&dictionary_contents));
  for (auto it = tracked_paths_.begin(); it != tracked_paths_.end(); ++it) {
    const std::string& initialized_path = it->first;
    const TrackedPreference* initialized_preference = it->second.get();
    const base::Value* value =
        pref_store_contents.FindByDottedPath(initialized_path);
    initialized_preference->OnNewValue(value, hash_store_transaction.get());
  }
}

// Marks |path| has having changed if it is part of |tracked_paths_|. A new hash
// will be stored for it the next time FilterSerializeData() is invoked.
void PrefHashFilter::FilterUpdate(std::string_view path) {
  auto it = tracked_paths_.find(path);
  if (it != tracked_paths_.end())
    changed_paths_.insert(std::make_pair(path, it->second.get()));
}

// Updates the stored hashes for |changed_paths_| before serializing data to
// disk. This is required as storing the hash everytime a pref's value changes
// is too expensive (see perf regression @ http://crbug.com/331273).
PrefFilter::OnWriteCallbackPair PrefHashFilter::FilterSerializeData(
    base::Value::Dict& pref_store_contents) {
  // Generate the callback pair before clearing |changed_paths_|.
  PrefFilter::OnWriteCallbackPair callback_pair =
      GetOnWriteSynchronousCallbacks(pref_store_contents);

  if (!changed_paths_.empty()) {
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
        const base::Value* value =
            pref_store_contents.FindByDottedPath(changed_path);
        changed_preference->OnNewValue(value, hash_store_transaction.get());
      }
      changed_paths_.clear();
    }
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
    base::Value::Dict pref_store_contents,
    bool prefs_altered) {
  bool did_reset = false;
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

    CleanupDeprecatedTrackedPreferences(pref_store_contents,
                                        hash_store_transaction.get());

    for (auto it = tracked_paths_.begin(); it != tracked_paths_.end(); ++it) {
      if (it->second->EnforceAndReport(
              pref_store_contents, hash_store_transaction.get(),
              external_validation_hash_store_transaction.get())) {
        did_reset = true;
        prefs_altered = true;
      }
    }
    if (hash_store_transaction->StampSuperMac())
      prefs_altered = true;
  }

  if (did_reset) {
    pref_store_contents.SetByDottedPath(
        user_prefs::kPreferenceResetTime,
        base::NumberToString(base::Time::Now().ToInternalValue()));
    FilterUpdate(user_prefs::kPreferenceResetTime);

    if (reset_on_load_observer_)
      reset_on_load_observer_->OnResetOnLoad();
  }
  reset_on_load_observer_.reset();

  std::move(post_filter_on_load_callback)
      .Run(std::move(pref_store_contents), prefs_altered);
}

base::WeakPtr<InterceptablePrefFilter> PrefHashFilter::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
void PrefHashFilter::ClearFromExternalStore(
    HashStoreContents* external_validation_hash_store_contents,
    const base::Value::Dict* changed_paths_and_macs) {
  DCHECK(changed_paths_and_macs);
  DCHECK(!changed_paths_and_macs->empty());

  for (const auto item : *changed_paths_and_macs) {
    external_validation_hash_store_contents->RemoveEntry(item.first);
  }
}

// static
void PrefHashFilter::FlushToExternalStore(
    std::unique_ptr<HashStoreContents> external_validation_hash_store_contents,
    std::unique_ptr<base::Value::Dict> changed_paths_and_macs,
    bool write_success) {
  DCHECK(changed_paths_and_macs);
  DCHECK(!changed_paths_and_macs->empty());
  DCHECK(external_validation_hash_store_contents);
  if (!write_success)
    return;

  for (const auto item : *changed_paths_and_macs) {
    const std::string& changed_path = item.first;

    if (item.second.is_dict()) {
      const base::Value::Dict& split_values = item.second.GetDict();
      for (const auto inner_item : split_values) {
        const std::string* mac = inner_item.second.GetIfString();
        bool is_string = !!mac;
        DCHECK(is_string);

        external_validation_hash_store_contents->SetSplitMac(
            changed_path, inner_item.first, *mac);
      }
    } else {
      DCHECK(item.second.is_string());
      external_validation_hash_store_contents->SetMac(changed_path,
                                                      item.second.GetString());
    }
  }
}

PrefFilter::OnWriteCallbackPair PrefHashFilter::GetOnWriteSynchronousCallbacks(
    base::Value::Dict& pref_store_contents) {
  if (changed_paths_.empty() || !external_validation_hash_store_pair_) {
    return std::make_pair(base::OnceClosure(),
                          base::OnceCallback<void(bool success)>());
  }

  auto changed_paths_macs = std::make_unique<base::Value::Dict>();

  for (ChangedPathsMap::const_iterator it = changed_paths_.begin();
       it != changed_paths_.end(); ++it) {
    const std::string& changed_path = it->first;
    const TrackedPreference* changed_preference = it->second;

    switch (changed_preference->GetType()) {
      case TrackedPreferenceType::ATOMIC: {
        const base::Value* new_value =
            pref_store_contents.FindByDottedPath(changed_path);
        changed_paths_macs->Set(
            changed_path,
            external_validation_hash_store_pair_->first->ComputeMac(
                changed_path, new_value));
        break;
      }
      case TrackedPreferenceType::SPLIT: {
        const base::Value::Dict* dict =
            pref_store_contents.FindDictByDottedPath(changed_path);
        changed_paths_macs->Set(
            changed_path,
            external_validation_hash_store_pair_->first->ComputeSplitMacs(
                changed_path, dict));
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
  base::Value::Dict* raw_changed_paths_macs = changed_paths_macs.get();

  return std::make_pair(
      base::BindOnce(&ClearFromExternalStore, base::Unretained(raw_contents),
                     base::Unretained(raw_changed_paths_macs)),
      base::BindOnce(&FlushToExternalStore, std::move(hash_store_contents_copy),
                     std::move(changed_paths_macs)));
}

// static
void PrefHashFilter::SetDeprecatedPrefsForTesting(
    const std::vector<const char*>& deprecated_prefs) {
  *GetDeprecatedPrefs() = deprecated_prefs;
}
