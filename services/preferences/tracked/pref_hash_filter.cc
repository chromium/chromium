// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/pref_hash_filter.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "services/preferences/tracked/dictionary_hash_store_contents.h"
#include "services/preferences/tracked/features.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"
#include "services/preferences/tracked/tracked_atomic_preference.h"
#include "services/preferences/tracked/tracked_split_preference.h"

namespace {

std::vector<const char*>* GetDeprecatedPrefs() {
  // Add deprecated previously tracked preferences below for them to be cleaned
  // up from both the pref files and the hash store.
  static base::NoDestructor<std::vector<const char*>> prefs({
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
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      // Deprecated 05/2025. Remove after 05/2026.
      "module_blocklist_cache_md5_digest",
#endif
  });

  return prefs.get();
}

void CleanupDeprecatedTrackedPreferences(
    base::Value::Dict& pref_store_contents,
    PrefHashStoreTransaction* hash_store_transaction) {
  for (const char* key : *GetDeprecatedPrefs()) {
    pref_store_contents.RemoveByDottedPath(key);
    hash_store_transaction->ClearHash(key);
  }
}

const char* GetValueTypeString(base::Value::Type type) {
  using Type = base::Value::Type;
  switch (type) {
    case Type::NONE:
      return "None";
    case Type::BOOLEAN:
      return "Boolean";
    case Type::INTEGER:
      return "Integer";
    case Type::DOUBLE:
      return "Double";
    case Type::STRING:
      return "String";
    case Type::BINARY:
      return "Binary";
    case Type::DICT:
      return "Dictionary";
    case Type::LIST:
      return "List";
  }
  return "Unknown";
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
    size_t reporting_ids_count,
    os_crypt_async::OSCryptAsync* os_crypt)
    : pref_hash_store_(std::move(pref_hash_store)),
      external_validation_hash_store_pair_(
          external_validation_hash_store_pair.first
              ? std::make_optional(
                    std::move(external_validation_hash_store_pair))
              : std::nullopt),
      reset_on_load_observer_(std::move(reset_on_load_observer)),
      delegate_(std::move(delegate)),
      reporting_ids_count_(reporting_ids_count),
      deferred_task_runner_(
          base::MakeRefCounted<base::DeferredSequencedTaskRunner>(
              base::SequencedTaskRunner::GetCurrentDefault())),
      encrypted_hashing_enabled_(
          base::FeatureList::IsEnabled(tracked::kEncryptedPrefHashing)) {
  DCHECK(pref_hash_store_);
  DCHECK_GE(reporting_ids_count, tracked_preferences.size());
  // Verify that, if |external_validation_hash_store_pair_| is present, both its
  // items are non-null.
  DCHECK(!external_validation_hash_store_pair_.has_value() ||
         (external_validation_hash_store_pair_->first &&
          external_validation_hash_store_pair_->second));

  if (encrypted_hashing_enabled_ && os_crypt) {
    os_crypt->GetInstance(
        base::BindOnce(&InterceptablePrefFilter::OnEncryptorReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }
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

void PrefHashFilter::SetPrefService(PrefService* pref_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    CHECK_IS_TEST();
  }
  pref_service_ = pref_service;
}

// static
void PrefHashFilter::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // See GetResetTime for why this is a StringPref and not Int64Pref.
  registry->RegisterStringPref(
      user_prefs::kPreferenceResetTime,
      base::NumberToString(base::Time().ToInternalValue()));
  // TODO(crbug.com/454827188): Use delegate to handle event messaging instead
  // of using this pref, kTrackedPreferencesReset.
  registry->RegisterListPref(user_prefs::kTrackedPreferencesReset);
  // Register the preference to trigger a flush to disk.
  // It's a string preference to store a timestamp.
  registry->RegisterStringPref(
      user_prefs::kScheduleToFlushToDisk,
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
  }
  return base::Time::FromInternalValue(internal_value);
}

// static
void PrefHashFilter::ClearResetTime(PrefService* user_prefs) {
  user_prefs->ClearPref(user_prefs::kPreferenceResetTime);
}

// static
void PrefHashFilter::SetResetTimeForTesting(PrefService* user_prefs,
                                            base::Time time) {
  user_prefs->SetString(user_prefs::kPreferenceResetTime,
                        base::NumberToString(time.ToInternalValue()));
}

void PrefHashFilter::Initialize(base::Value::Dict& pref_store_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DictionaryHashStoreContents dictionary_contents(pref_store_contents);
  std::unique_ptr<PrefHashStoreTransaction> hash_store_transaction(
      pref_hash_store_->BeginTransaction(&dictionary_contents));
  for (auto it = tracked_paths_.begin(); it != tracked_paths_.end(); ++it) {
    const std::string& initialized_path = it->first;
    const TrackedPreference* initialized_preference = it->second.get();
    const base::Value* value =
        pref_store_contents.FindByDottedPath(initialized_path);
    // Initialize calls the 2-arg compatibility overload of
    // TrackedPreference::OnNewValue. Because at this point, the encryptor is
    // highly likely not ready yet.
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

void PrefHashFilter::OnEncryptorReceived(os_crypt_async::Encryptor encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encrypted_hashing_enabled_ && deferred_task_runner_);
  encryptor_.emplace(std::move(encryptor));
  deferred_task_runner_->Start();
}

// Updates the stored hashes for |changed_paths_| before serializing data to
// disk. This is required as storing the hash everytime a pref's value changes
// is too expensive (see perf regression @ http://crbug.com/331273).
PrefFilter::OnWriteCallbackPair PrefHashFilter::FilterSerializeData(
    base::Value::Dict& pref_store_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PrefFilter::OnWriteCallbackPair callback_pair =
      GetOnWriteSynchronousCallbacks(pref_store_contents);

  if (!changed_paths_.empty()) {
    const os_crypt_async::Encryptor* current_encryptor_ptr =
        encryptor_.has_value() ? &encryptor_.value() : nullptr;

    DictionaryHashStoreContents dictionary_contents(pref_store_contents);
    std::unique_ptr<PrefHashStoreTransaction> hash_store_transaction(
        pref_hash_store_->BeginTransaction(&dictionary_contents,
                                           current_encryptor_ptr));

    std::unique_ptr<PrefHashStoreTransaction>
        external_validation_hash_store_transaction;
    if (external_validation_hash_store_pair_) {
      external_validation_hash_store_transaction =
          external_validation_hash_store_pair_->first->BeginTransaction(
              external_validation_hash_store_pair_->second.get(),
              current_encryptor_ptr);
    }

    auto process_paths = [&](const auto& paths_container) {
      for (const auto& [path, preference] : paths_container) {
        const base::Value* value = pref_store_contents.FindByDottedPath(path);
        preference->OnNewValue(value, hash_store_transaction.get(),
                               current_encryptor_ptr);
      }
    };
    // If the encryptor is available, we must re-hash all tracked preferences to
    // ensure they have an encrypted hash. This is the fallback step for
    // profiles that were created before this feature was enabled.
    if (current_encryptor_ptr) {
      process_paths(tracked_paths_);
    } else {
      // If the feature is disabled/encryptor is not available, clear any
      // existing encrypted hashes.
      for (const auto& tracked_path : tracked_paths_) {
        hash_store_transaction->ClearEncryptedHash(tracked_path.first);
      }
      // If the encryptor isn't available, fall back to the old behavior of only
      // processing paths that have changed.
      process_paths(changed_paths_);
    }

    changed_paths_.clear();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<std::string> reset_paths;
  bool did_reset = false;

  // Perform the initial synchronous validation pass (without the encryptor).
  // This validates the super MAC and all individual preference MACs.
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
        reset_paths.insert(it->first);
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

    // Treat the setting of the reset time as a reset itself, so the async
    // validation will skip it. This prevents the "double reset" side effect.
    reset_paths.insert(user_prefs::kPreferenceResetTime);

    if (reset_on_load_observer_)
      reset_on_load_observer_->OnResetOnLoad();
  }
  reset_on_load_observer_.reset();

  // If encrypted hashing is on, post a deferred task to re-validate with the
  // encryptor once it's available. Pass a clone of the pref store contents
  // so the task operates on the exact state at load time. Also pass the list of
  // prefs already reset by the synchronous validation.
  if (encrypted_hashing_enabled_) {
    deferred_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&PrefHashFilter::DeferredEncryptorRevalidation,
                       weak_ptr_factory_.GetWeakPtr(),
                       pref_store_contents.Clone(), std::move(reset_paths)));
  } else {
    // No deferred task will be posted, so validation is complete.
    // Log metrics now.
    MaybeRecordTrackedPreferenceResetCount(pref_store_contents);

    // If the feature is disabled, and we have a test callback, run it now
    // as no deferred task will be posted.
    if (on_deferred_revalidation_complete_for_testing_) {
      std::move(on_deferred_revalidation_complete_for_testing_).Run();
    }
  }

  // The PrefService initialization is asynchronous. Post a task to
  // patch the live PrefService with the resets found in the
  // sync pass.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PrefHashFilter::UpdateTrackedPreferencesResetListInPrefStore,
          weak_ptr_factory_.GetWeakPtr(), pref_store_contents.Clone()));

  // Immediately call the callback with the original pref_store_contents to
  // allow startup to proceed without waiting for the encryptor.
  std::move(post_filter_on_load_callback)
      .Run(std::move(pref_store_contents), prefs_altered);
}

void PrefHashFilter::DeferredEncryptorRevalidation(
    base::Value::Dict pref_store_contents_at_load,
    const std::set<std::string>& already_reset_paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(encryptor_.has_value());
  const os_crypt_async::Encryptor* encryptor = &encryptor_.value();

  // The transaction operates on our cloned dictionary from load time.
  DictionaryHashStoreContents dictionary_contents(pref_store_contents_at_load);
  std::unique_ptr<PrefHashStoreTransaction> transaction(
      pref_hash_store_->BeginTransaction(&dictionary_contents, encryptor));

  // Schedule a write to disk by updating a tracked preference. This is done to
  // ensure the newly computed encrypted hashes are flushed to the pref store.
  // This value will be replaced by another reset time stamp if there are any
  // reset prefs.
  std::string_view pref_to_write(user_prefs::kScheduleToFlushToDisk);

  // First pass: Validate and reset any tampered preferences.
  for (const auto& [path, preference] : tracked_paths_) {
    // Skip re-validating any preference that was already reset during the
    // synchronous pass.
    if (already_reset_paths.count(path)) {
      continue;
    }
    const PrefService::Preference* pref = pref_service_->FindPreference(path);
    if (!pref) {
      continue;
    }

    const base::Value* value_at_load =
        pref_store_contents_at_load.FindByDottedPath(path);
    if (value_at_load && value_at_load->type() != pref->GetType()) {
      const std::string histogram_name =
          base::StrCat({"Settings.TrackedPreferences.TypeMismatch.Combination.",
                        GetValueTypeString(pref->GetType()), "To",
                        GetValueTypeString(value_at_load->type())});
      base::UmaHistogramExactLinear(
          histogram_name, preference->GetReportingId(), reporting_ids_count_);
      continue;
    } else {
      const base::Value* current_value = pref_service_->GetUserPrefValue(path);

      // Compare the current value from pref service and the value from the copy
      // of the loaded store. If the pref has been modified, we skip the
      // encryption hash check.
      if (current_value && value_at_load) {
        // Both values exist. Check if they are different.
        if (*current_value != *value_at_load) {
          continue;
        }
      } else if (current_value != value_at_load) {
        continue;
      }  // If we fall through eventually, this means both values are valid and
         // equal.
    }

    if (preference->EnforceAndReport(pref_store_contents_at_load,
                                     transaction.get(),
                                     nullptr /* external_tx */, encryptor)) {
      // The preference was invalid. Update the *live* preference with the
      // corrected value from the in-memory `pref_store_contents_at_load`
      // dictionary, which `EnforceAndReport` has already modified.
      const base::Value* corrected_value =
          pref_store_contents_at_load.FindByDottedPath(path);
      if (corrected_value) {
        pref_service_->Set(path, corrected_value->Clone());
      } else {
        // If the corrected value is null (meaning the whole preference was
        // corrupt and removed), then clear the live pref.
        pref_service_->ClearPref(path);
      }
      pref_to_write = user_prefs::kPreferenceResetTime;
    }
  }

  // If any preferences were reset, the `kTrackedPreferencesReset` list in
  // `pref_store_contents_at_load` has been updated. Propagate this change to
  // the live PrefService to ensure it's persisted.
  if (pref_to_write == user_prefs::kPreferenceResetTime) {
    UpdateTrackedPreferencesResetListInPrefStore(pref_store_contents_at_load);
  }

  // This is the final validation pass. Log metrics if we haven't already.
  MaybeRecordTrackedPreferenceResetCount(pref_store_contents_at_load);

  pref_service_->SetString(
      pref_to_write, base::NumberToString(base::Time::Now().ToInternalValue()));
  if (on_deferred_revalidation_complete_for_testing_) {
    std::move(on_deferred_revalidation_complete_for_testing_).Run();
  }
}

base::WeakPtr<InterceptablePrefFilter> PrefHashFilter::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PrefHashFilter::UpdateTrackedPreferencesResetListInPrefStore(
    const base::Value::Dict& pref_store_contents_at_load) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This task should only run after PrefService is created.
  if (!pref_service_) {
    // This path is hit by ProfilePrefStoreManagerTest, which reads the
    // store before a PrefService is associated with the filter.
    CHECK_IS_TEST();
    return;
  }

  const base::Value::List* reset_list = pref_store_contents_at_load.FindList(
      user_prefs::kTrackedPreferencesReset);
  if (reset_list) {
    pref_service_->SetList(user_prefs::kTrackedPreferencesReset,
                           reset_list->Clone());
  } else {
    pref_service_->ClearPref(user_prefs::kTrackedPreferencesReset);
  }
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

void PrefHashFilter::SetOnDeferredRevalidationCompleteForTesting(
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_deferred_revalidation_complete_for_testing_ = std::move(callback);
}

void PrefHashFilter::MaybeRecordTrackedPreferenceResetCount(
    const base::Value::Dict& pref_store_contents) {
  if (reset_metric_recorded_) {
    return;
  }
  const base::Value::List* reset_list =
      pref_store_contents.FindList(user_prefs::kTrackedPreferencesReset);
  UMA_HISTOGRAM_COUNTS_100("Settings.TrackedPreferenceResets.Count",
                           reset_list ? reset_list->size() : 0);
  reset_metric_recorded_ = true;
}

// static
void PrefHashFilter::SetDeprecatedPrefsForTesting(
    const std::vector<const char*>& deprecated_prefs) {
  *GetDeprecatedPrefs() = deprecated_prefs;
}
