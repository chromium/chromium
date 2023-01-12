// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_preferences_migration.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "components/prefs/pref_name_set.h"
#include "services/preferences/tracked/dictionary_hash_store_contents.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/interceptable_pref_filter.h"
#include "services/preferences/tracked/pref_hash_store.h"
#include "services/preferences/tracked/pref_hash_store_transaction.h"

namespace {

class TrackedPreferencesMigrator
    : public base::RefCounted<TrackedPreferencesMigrator> {
 public:
  enum PrefFilterID { UNPROTECTED_PREF_FILTER, PROTECTED_PREF_FILTER };

  TrackedPreferencesMigrator(
      const PrefNameSet& unprotected_pref_names,
      const PrefNameSet& protected_pref_names,
      const base::RepeatingCallback<void(const std::string& key)>&
          unprotected_store_cleaner,
      const base::RepeatingCallback<void(const std::string& key)>&
          protected_store_cleaner,
      const base::RepeatingCallback<void(base::OnceClosure)>&
          register_on_successful_unprotected_store_write_callback,
      const base::RepeatingCallback<void(base::OnceClosure)>&
          register_on_successful_protected_store_write_callback,
      std::unique_ptr<PrefHashStore> unprotected_pref_hash_store,
      std::unique_ptr<PrefHashStore> protected_pref_hash_store,
      InterceptablePrefFilter* unprotected_pref_filter,
      InterceptablePrefFilter* protected_pref_filter);

  TrackedPreferencesMigrator(const TrackedPreferencesMigrator&) = delete;
  TrackedPreferencesMigrator& operator=(const TrackedPreferencesMigrator&) =
      delete;

  // Stores the data coming in from the filter identified by |id| into this
  // class and then calls MigrateIfReady();
  void InterceptFilterOnLoad(
      PrefFilterID id,
      InterceptablePrefFilter::FinalizeFilterOnLoadCallback
          finalize_filter_on_load,
      base::Value::Dict prefs);

 private:
  friend class base::RefCounted<TrackedPreferencesMigrator>;

  ~TrackedPreferencesMigrator();

  // Proceeds with migration if both |unprotected_prefs_| and |protected_prefs_|
  // have been set.
  void MigrateIfReady();

  const PrefNameSet unprotected_pref_names_;
  const PrefNameSet protected_pref_names_;

  const base::RepeatingCallback<void(const std::string& key)>
      unprotected_store_cleaner_;
  const base::RepeatingCallback<void(const std::string& key)>
      protected_store_cleaner_;
  const base::RepeatingCallback<void(base::OnceClosure)>
      register_on_successful_unprotected_store_write_callback_;
  const base::RepeatingCallback<void(base::OnceClosure)>
      register_on_successful_protected_store_write_callback_;

  InterceptablePrefFilter::FinalizeFilterOnLoadCallback
      finalize_unprotected_filter_on_load_;
  InterceptablePrefFilter::FinalizeFilterOnLoadCallback
      finalize_protected_filter_on_load_;

  std::unique_ptr<PrefHashStore> unprotected_pref_hash_store_;
  std::unique_ptr<PrefHashStore> protected_pref_hash_store_;

  std::unique_ptr<base::Value::Dict> unprotected_prefs_;
  std::unique_ptr<base::Value::Dict> protected_prefs_;
};

// Invokes |store_cleaner| for every |keys_to_clean|.
void CleanupPrefStore(
    const base::RepeatingCallback<void(const std::string& key)>& store_cleaner,
    const PrefNameSet& keys_to_clean) {
  for (const auto& key : keys_to_clean) {
    store_cleaner.Run(key);
  }
}

// If |wait_for_commit_to_destination_store|: schedules (via
// |register_on_successful_destination_store_write_callback|) a cleanup of the
// |keys_to_clean| from the source pref store (through |source_store_cleaner|)
// once the destination pref store they were migrated to was successfully
// written to disk. Otherwise, executes the cleanup right away.
void ScheduleSourcePrefStoreCleanup(
    const base::RepeatingCallback<void(base::OnceClosure)>&
        register_on_successful_destination_store_write_callback,
    const base::RepeatingCallback<void(const std::string& key)>&
        source_store_cleaner,
    const PrefNameSet& keys_to_clean,
    bool wait_for_commit_to_destination_store) {
  DCHECK(!keys_to_clean.empty());
  if (wait_for_commit_to_destination_store) {
    register_on_successful_destination_store_write_callback.Run(
        base::BindOnce(&CleanupPrefStore, source_store_cleaner, keys_to_clean));
  } else {
    CleanupPrefStore(source_store_cleaner, keys_to_clean);
  }
}

// Removes hashes for |migrated_pref_names| from |origin_pref_store| using
// the configuration/implementation in |origin_pref_hash_store|.
void CleanupMigratedHashes(const PrefNameSet& migrated_pref_names,
                           PrefHashStore* origin_pref_hash_store,
                           base::Value::Dict& origin_pref_store) {
  DictionaryHashStoreContents dictionary_contents(origin_pref_store);
  std::unique_ptr<PrefHashStoreTransaction> transaction(
      origin_pref_hash_store->BeginTransaction(&dictionary_contents));
  for (std::set<std::string>::const_iterator it = migrated_pref_names.begin();
       it != migrated_pref_names.end(); ++it) {
    transaction->ClearHash(*it);
  }
}

// Copies the value of each pref in |pref_names| which is set in |old_store|,
// but not in |new_store| into |new_store|. Sets |old_store_needs_cleanup| to
// true if any old duplicates remain in |old_store| and sets |new_store_altered|
// to true if any value was copied to |new_store|.
void MigratePrefsFromOldToNewStore(const PrefNameSet& pref_names,
                                   base::Value::Dict& old_store,
                                   base::Value::Dict& new_store,
                                   PrefHashStore* new_hash_store,
                                   bool* old_store_needs_cleanup,
                                   bool* new_store_altered) {
  const base::Value::Dict* old_hash_store_contents =
      DictionaryHashStoreContents(old_store).GetContents();
  DictionaryHashStoreContents dictionary_contents(new_store);
  std::unique_ptr<PrefHashStoreTransaction> new_hash_store_transaction(
      new_hash_store->BeginTransaction(&dictionary_contents));

  for (std::set<std::string>::const_iterator it = pref_names.begin();
       it != pref_names.end(); ++it) {
    const std::string& pref_name = *it;

    // If the destination does not have a hash for this pref we will
    // unconditionally attempt to move it.
    bool destination_hash_missing =
        !new_hash_store_transaction->HasHash(pref_name);
    // If we migrate the value we will also attempt to migrate the hash.
    bool migrated_value = false;
    if (const base::Value* value_in_old_store =
            old_store.FindByDottedPath(pref_name)) {
      // Whether this value ends up being copied below or was left behind by a
      // previous incomplete migration, it should be cleaned up.
      *old_store_needs_cleanup = true;

      if (!new_store.FindByDottedPath(pref_name)) {
        // Copy the value from |old_store| to |new_store| rather than moving it
        // to avoid data loss should |old_store| be flushed to disk without
        // |new_store| having equivalently been successfully flushed to disk
        // (e.g., on crash or in cases where |new_store| is read-only following
        // a read error on startup).
        new_store.SetByDottedPath(pref_name, value_in_old_store->Clone());
        migrated_value = true;
        *new_store_altered = true;
      }
    }

    if (destination_hash_missing || migrated_value) {
      const base::Value* old_hash = nullptr;
      if (old_hash_store_contents)
        old_hash = old_hash_store_contents->FindByDottedPath(pref_name);
      if (old_hash) {
        new_hash_store_transaction->ImportHash(pref_name, old_hash);
        *new_store_altered = true;
      } else if (!destination_hash_missing) {
        // Do not allow values to be migrated without MACs if the destination
        // already has a MAC (http://crbug.com/414554). Remove the migrated
        // value in order to provide the same no-op behaviour as if the pref was
        // added to the wrong file when there was already a value for
        // |pref_name| in |new_store|.
        new_store.RemoveByDottedPath(pref_name);
        *new_store_altered = true;
      }
    }
  }
}

TrackedPreferencesMigrator::TrackedPreferencesMigrator(
    const PrefNameSet& unprotected_pref_names,
    const PrefNameSet& protected_pref_names,
    const base::RepeatingCallback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::RepeatingCallback<void(const std::string& key)>&
        protected_store_cleaner,
    const base::RepeatingCallback<void(base::OnceClosure)>&
        register_on_successful_unprotected_store_write_callback,
    const base::RepeatingCallback<void(base::OnceClosure)>&
        register_on_successful_protected_store_write_callback,
    std::unique_ptr<PrefHashStore> unprotected_pref_hash_store,
    std::unique_ptr<PrefHashStore> protected_pref_hash_store,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter)
    : unprotected_pref_names_(unprotected_pref_names),
      protected_pref_names_(protected_pref_names),
      unprotected_store_cleaner_(unprotected_store_cleaner),
      protected_store_cleaner_(protected_store_cleaner),
      register_on_successful_unprotected_store_write_callback_(
          register_on_successful_unprotected_store_write_callback),
      register_on_successful_protected_store_write_callback_(
          register_on_successful_protected_store_write_callback),
      unprotected_pref_hash_store_(std::move(unprotected_pref_hash_store)),
      protected_pref_hash_store_(std::move(protected_pref_hash_store)) {}

TrackedPreferencesMigrator::~TrackedPreferencesMigrator() {}

void TrackedPreferencesMigrator::InterceptFilterOnLoad(
    PrefFilterID id,
    InterceptablePrefFilter::FinalizeFilterOnLoadCallback
        finalize_filter_on_load,
    base::Value::Dict prefs) {
  auto prefs_migration = std::make_unique<base::Value::Dict>(std::move(prefs));
  switch (id) {
    case UNPROTECTED_PREF_FILTER:
      finalize_unprotected_filter_on_load_ = std::move(finalize_filter_on_load);
      unprotected_prefs_ = std::move(prefs_migration);
      break;
    case PROTECTED_PREF_FILTER:
      finalize_protected_filter_on_load_ = std::move(finalize_filter_on_load);
      protected_prefs_ = std::move(prefs_migration);
      break;
  }

  MigrateIfReady();
}

void TrackedPreferencesMigrator::MigrateIfReady() {
  // Wait for both stores to have been read before proceeding.
  if (!protected_prefs_ || !unprotected_prefs_)
    return;

  bool protected_prefs_need_cleanup = false;
  bool unprotected_prefs_altered = false;
  MigratePrefsFromOldToNewStore(
      unprotected_pref_names_, *protected_prefs_, *unprotected_prefs_,
      unprotected_pref_hash_store_.get(), &protected_prefs_need_cleanup,
      &unprotected_prefs_altered);
  bool unprotected_prefs_need_cleanup = false;
  bool protected_prefs_altered = false;
  MigratePrefsFromOldToNewStore(
      protected_pref_names_, *unprotected_prefs_, *protected_prefs_,
      protected_pref_hash_store_.get(), &unprotected_prefs_need_cleanup,
      &protected_prefs_altered);

  if (!unprotected_prefs_altered && !protected_prefs_altered) {
    // Clean up any MACs that might have been previously migrated from the
    // various stores. It's safe to leave them behind for a little while as they
    // will be ignored unless the corresponding value is _also_ present. The
    // cleanup must be deferred until the MACs have been written to their target
    // stores, and doing so in a subsequent launch is easier than within the
    // same process.
    CleanupMigratedHashes(unprotected_pref_names_,
                          protected_pref_hash_store_.get(), *protected_prefs_);
    CleanupMigratedHashes(protected_pref_names_,
                          unprotected_pref_hash_store_.get(),
                          *unprotected_prefs_);
  }

  // Hand the processed prefs back to their respective filters.
  std::unique_ptr<base::Value::Dict> unprotected_prefs =
      std::move(unprotected_prefs_);
  std::move(finalize_unprotected_filter_on_load_)
      .Run(std::move(*unprotected_prefs), unprotected_prefs_altered);
  std::unique_ptr<base::Value::Dict> protected_prefs =
      std::move(protected_prefs_);
  std::move(finalize_protected_filter_on_load_)
      .Run(std::move(*protected_prefs), protected_prefs_altered);

  if (unprotected_prefs_need_cleanup) {
    // Schedule a cleanup of the |protected_pref_names_| from the unprotected
    // prefs once the protected prefs were successfully written to disk (or
    // do it immediately if |!protected_prefs_altered|).
    ScheduleSourcePrefStoreCleanup(
        register_on_successful_protected_store_write_callback_,
        unprotected_store_cleaner_, protected_pref_names_,
        protected_prefs_altered);
  }

  if (protected_prefs_need_cleanup) {
    // Schedule a cleanup of the |unprotected_pref_names_| from the protected
    // prefs once the unprotected prefs were successfully written to disk (or
    // do it immediately if |!unprotected_prefs_altered|).
    ScheduleSourcePrefStoreCleanup(
        register_on_successful_unprotected_store_write_callback_,
        protected_store_cleaner_, unprotected_pref_names_,
        unprotected_prefs_altered);
  }
}

}  // namespace

void SetupTrackedPreferencesMigration(
    const PrefNameSet& unprotected_pref_names,
    const PrefNameSet& protected_pref_names,
    const base::RepeatingCallback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::RepeatingCallback<void(const std::string& key)>&
        protected_store_cleaner,
    const base::RepeatingCallback<void(base::OnceClosure)>&
        register_on_successful_unprotected_store_write_callback,
    const base::RepeatingCallback<void(base::OnceClosure)>&
        register_on_successful_protected_store_write_callback,
    std::unique_ptr<PrefHashStore> unprotected_pref_hash_store,
    std::unique_ptr<PrefHashStore> protected_pref_hash_store,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter) {
  auto prefs_migrator = base::MakeRefCounted<TrackedPreferencesMigrator>(
      unprotected_pref_names, protected_pref_names, unprotected_store_cleaner,
      protected_store_cleaner,
      register_on_successful_unprotected_store_write_callback,
      register_on_successful_protected_store_write_callback,
      std::move(unprotected_pref_hash_store),
      std::move(protected_pref_hash_store), unprotected_pref_filter,
      protected_pref_filter);

  // The callbacks bound below will own this TrackedPreferencesMigrator by
  // reference.
  unprotected_pref_filter->InterceptNextFilterOnLoad(base::BindOnce(
      &TrackedPreferencesMigrator::InterceptFilterOnLoad, prefs_migrator,
      TrackedPreferencesMigrator::UNPROTECTED_PREF_FILTER));
  protected_pref_filter->InterceptNextFilterOnLoad(base::BindOnce(
      &TrackedPreferencesMigrator::InterceptFilterOnLoad, prefs_migrator,
      TrackedPreferencesMigrator::PROTECTED_PREF_FILTER));
}
