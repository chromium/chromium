// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_FILTER_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_FILTER_H_

#include <stddef.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/values.h"
#include "components/prefs/transparent_unordered_string_map.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/interceptable_pref_filter.h"
#include "services/preferences/tracked/pref_hash_store.h"
#include "services/preferences/tracked/tracked_preference.h"

class PrefService;

namespace base {
class Time;
}  // namespace base

namespace prefs::mojom {
class TrackedPreferenceValidationDelegate;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Intercepts preference values as they are loaded from disk and verifies them
// using a PrefHashStore. Keeps the PrefHashStore contents up to date as values
// are changed.
class PrefHashFilter final : public InterceptablePrefFilter {
 public:
  using StoreContentsPair = std::pair<std::unique_ptr<PrefHashStore>,
                                      std::unique_ptr<HashStoreContents>>;

  // A map from changed paths to their corresponding TrackedPreferences (which
  // aren't owned by this map).
  using ChangedPathsMap =
      std::map<std::string, raw_ptr<const TrackedPreference, CtnExperimental>>;

  // Constructs a PrefHashFilter tracking the specified |tracked_preferences|
  // using |pref_hash_store| to check/store hashes. An optional |delegate| is
  // notified of the status of each preference as it is checked.
  // If |reset_on_load_observer| is provided, it will be notified if a reset
  // occurs in FilterOnLoad.
  // |reporting_ids_count| is the count of all possible IDs (possibly greater
  // than |tracked_preferences.size()|).
  // |external_validation_hash_store_pair_| will be used (if non-null) to
  // perform extra validations without triggering resets.
  // |os_crypt| provides an asynchronous interface to OS-level encryption for
  // storing an additional encrypted hash if kEncryptedPrefHashing is enabled.
  // The encryptor could be null on start-up but it will be retrieved
  // asynchronously.
  PrefHashFilter(
      std::unique_ptr<PrefHashStore> pref_hash_store,
      StoreContentsPair external_validation_hash_store_pair_,
      const std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>&
          tracked_preferences,
      mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
          reset_on_load_observer,
      scoped_refptr<base::RefCountedData<
          mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>
          delegate,
      size_t reporting_ids_count,
      os_crypt_async::OSCryptAsync* os_crypt);

  PrefHashFilter(const PrefHashFilter&) = delete;
  PrefHashFilter& operator=(const PrefHashFilter&) = delete;

  ~PrefHashFilter() override;

  // Registers required user preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Retrieves the time of the last reset event, if any, for the provided user
  // preferences. If no reset has occurred, Returns a null |Time|.
  static base::Time GetResetTime(PrefService* user_prefs);

  // Clears the time of the last reset event, if any, for the provided user
  // preferences.
  static void ClearResetTime(PrefService* user_prefs);

  // Sets the time of the last reset event to now.
  static void SetResetTimeForTesting(PrefService* user_prefs, base::Time time);

  // Initializes the PrefHashStore with hashes of the tracked preferences in
  // |pref_store_contents|. |pref_store_contents| will be the |storage| passed
  // to PrefHashStore::BeginTransaction().
  void Initialize(base::Value::Dict& pref_store_contents);

  // PrefFilter remaining implementation.
  void FilterUpdate(std::string_view path) override;
  OnWriteCallbackPair FilterSerializeData(
      base::Value::Dict& pref_store_contents) override;

  void OnStoreDeletionFromDisk() override;

  static void SetDeprecatedPrefsForTesting(
      const std::vector<const char*>& deprecated_prefs);

  // Sets a callback to be run for testing purposes when deferred revalidation
  // is complete.
  void SetOnDeferredRevalidationCompleteForTesting(base::OnceClosure callback);

  // Sets the PrefService that owns this filter. This must be called after
  // construction and before any deferred tasks can run that might need it.
  void SetPrefService(PrefService* pref_service) override;

 private:
  // Friend fixtures for unit testing.
  FRIEND_TEST_ALL_PREFIXES(PrefHashFilterTest,
                           RecordTrackedPreferenceResetCount_NoResets);
  FRIEND_TEST_ALL_PREFIXES(PrefHashFilterTest,
                           RecordTrackedPreferenceResetCount_WithResets);
  FRIEND_TEST_ALL_PREFIXES(PrefHashFilterTest,
                           MaybeRecordTrackedPreferenceResetCount_LogsOnce);
  // InterceptablePrefFilter implementation.
  void FinalizeFilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      base::Value::Dict pref_store_contents,
      bool prefs_altered) override;

  base::WeakPtr<InterceptablePrefFilter> AsWeakPtr() override;

  // Helper function to generate FilterSerializeData()'s pre-write and
  // post-write callbacks. The returned callbacks are thread-safe.
  OnWriteCallbackPair GetOnWriteSynchronousCallbacks(
      base::Value::Dict& pref_store_contents);

  // Clears the MACs contained in |external_validation_hash_store_contents|
  // which are present in |paths_to_clear|.
  static void ClearFromExternalStore(
      HashStoreContents* external_validation_hash_store_contents,
      const base::Value::Dict* changed_paths_and_macs);

  // Flushes the MACs contained in |changed_paths_and_mac| to
  // external_hash_store_contents if |write_success|, otherwise discards the
  // changes.
  static void FlushToExternalStore(
      std::unique_ptr<HashStoreContents> external_hash_store_contents,
      std::unique_ptr<base::Value::Dict> changed_paths_and_macs,
      bool write_success);

  void OnEncryptorReceived(os_crypt_async::Encryptor encryptor) override;

  // Performs the deferred work of re-validating preferences after the
  // encryptor has been fetched. This is posted from FinalizeFilterOnLoad.
  // |pref_store_contents_at_load| is a copy of the preference dictionary as
  //     it was read from disk, before any resets. This is used to validate
  //     against the original state.
  // |already_reset_paths| is a set of preference paths that were already
  //     found to be invalid and were reset during the initial synchronous
  //     validation pass. These paths will be skipped.
  void DeferredEncryptorRevalidation(
      base::Value::Dict pref_store_contents_at_load,
      const std::set<std::string>& already_reset_paths);

  // Logs the metric of the number of preferences that were reset. Ensures this
  // metric is only logged once per filter instance.
  void MaybeRecordTrackedPreferenceResetCount(
      const base::Value::Dict& pref_store_contents);

  // Applies resets found during any validation pass to the live PrefService.
  // This is posted as a task to run after PrefService initialization is
  // complete.
  void UpdateTrackedPreferencesResetListInPrefStore(
      const base::Value::Dict& pref_store_contents_at_load);

  // Callback to be invoked only once (and subsequently reset) on the next
  // FilterOnLoad event. It will be allowed to modify the |prefs| handed to
  // FilterOnLoad before handing them back to this PrefHashFilter.
  FilterOnLoadInterceptor filter_on_load_interceptor_;

  // A map of paths to TrackedPreferences; this map owns this individual
  // TrackedPreference objects.
  using TrackedPreferencesMap =
      TransparentUnorderedStringMap<std::unique_ptr<TrackedPreference>>;

  std::unique_ptr<PrefHashStore> pref_hash_store_;

  // A store and contents on which to perform extra validations without
  // triggering resets.
  // Will be null if the platform does not support external validation.
  std::optional<StoreContentsPair> external_validation_hash_store_pair_;

  // Notified if a reset occurs in a call to FilterOnLoad.
  mojo::Remote<prefs::mojom::ResetOnLoadObserver> reset_on_load_observer_;
  scoped_refptr<base::RefCountedData<
      mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>
      delegate_;

  TrackedPreferencesMap tracked_paths_;

  // The set of all paths whose value has changed since the last call to
  // FilterSerializeData.
  ChangedPathsMap changed_paths_;

  // The total number of reporting IDs.
  const size_t reporting_ids_count_;

  // A flag that recordes if the reset pref has been recorded.
  bool reset_metric_recorded_ = false;

  // A deferred task runner to defer and start the async encryptor related
  // validation task.
  scoped_refptr<base::DeferredSequencedTaskRunner> deferred_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // A raw pointer to the PrefService that owns this filter. This is safe
  // because the PrefService is guaranteed to outlive this filter.
  raw_ptr<PrefService> pref_service_ = nullptr;

  const bool encrypted_hashing_enabled_;
  std::optional<os_crypt_async::Encryptor> encryptor_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A callback to be run for testing purposes when deferred revalidation is
  // complete.
  base::OnceClosure on_deferred_revalidation_complete_for_testing_;

  base::WeakPtrFactory<PrefHashFilter> weak_ptr_factory_{this};
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_FILTER_H_
