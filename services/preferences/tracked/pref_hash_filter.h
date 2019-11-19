// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_PREF_HASH_FILTER_H_
#define SERVICES_PREFERENCES_TRACKED_PREF_HASH_FILTER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/preferences/tracked/hash_store_contents.h"
#include "services/preferences/tracked/interceptable_pref_filter.h"
#include "services/preferences/tracked/pref_hash_store.h"
#include "services/preferences/tracked/tracked_preference.h"

class PrefService;

namespace base {
class DictionaryValue;
class Time;
}  // namespace base

namespace prefs {
namespace mojom {
class TrackedPreferenceValidationDelegate;
}
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Intercepts preference values as they are loaded from disk and verifies them
// using a PrefHashStore. Keeps the PrefHashStore contents up to date as values
// are changed.
class PrefHashFilter : public InterceptablePrefFilter {
 public:
  using StoreContentsPair = std::pair<std::unique_ptr<PrefHashStore>,
                                      std::unique_ptr<HashStoreContents>>;

  // Constructs a PrefHashFilter tracking the specified |tracked_preferences|
  // using |pref_hash_store| to check/store hashes. An optional |delegate| is
  // notified of the status of each preference as it is checked.
  // If |reset_on_load_observer| is provided, it will be notified if a reset
  // occurs in FilterOnLoad.
  // |reporting_ids_count| is the count of all possible IDs (possibly greater
  // than |tracked_preferences.size()|).
  // |external_validation_hash_store_pair_| will be used (if non-null) to
  // perform extra validations without triggering resets.
  PrefHashFilter(std::unique_ptr<PrefHashStore> pref_hash_store,
                 StoreContentsPair external_validation_hash_store_pair_,
                 const std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>&
                     tracked_preferences,
                 mojo::PendingRemote<prefs::mojom::ResetOnLoadObserver>
                     reset_on_load_observer,
                 prefs::mojom::TrackedPreferenceValidationDelegate* delegate,
                 size_t reporting_ids_count);

  ~PrefHashFilter() override;

  // Registers required user preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Retrieves the time of the last reset event, if any, for the provided user
  // preferences. If no reset has occurred, Returns a null |Time|.
  static base::Time GetResetTime(PrefService* user_prefs);

  // Clears the time of the last reset event, if any, for the provided user
  // preferences.
  static void ClearResetTime(PrefService* user_prefs);

  // Initializes the PrefHashStore with hashes of the tracked preferences in
  // |pref_store_contents|. |pref_store_contents| will be the |storage| passed
  // to PrefHashStore::BeginTransaction().
  void Initialize(base::DictionaryValue* pref_store_contents);

  // PrefFilter remaining implementation.
  void FilterUpdate(const std::string& path) override;
  OnWriteCallbackPair FilterSerializeData(
      base::DictionaryValue* pref_store_contents) override;

  void OnStoreDeletionFromDisk() override;

 private:
  // InterceptablePrefFilter implementation.
  void FinalizeFilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      std::unique_ptr<base::DictionaryValue> pref_store_contents,
      bool prefs_altered) override;

  // Helper function to generate FilterSerializeData()'s pre-write and
  // post-write callbacks. The returned callbacks are thread-safe.
  OnWriteCallbackPair GetOnWriteSynchronousCallbacks(
      base::DictionaryValue* pref_store_contents);

  // Clears the MACs contained in |external_validation_hash_store_contents|
  // which are present in |paths_to_clear|.
  static void ClearFromExternalStore(
      HashStoreContents* external_validation_hash_store_contents,
      const base::DictionaryValue* changed_paths_and_macs);

  // Flushes the MACs contained in |changed_paths_and_mac| to
  // external_hash_store_contents if |write_success|, otherwise discards the
  // changes.
  static void FlushToExternalStore(
      std::unique_ptr<HashStoreContents> external_hash_store_contents,
      std::unique_ptr<base::DictionaryValue> changed_paths_and_macs,
      bool write_success);

  // Callback to be invoked only once (and subsequently reset) on the next
  // FilterOnLoad event. It will be allowed to modify the |prefs| handed to
  // FilterOnLoad before handing them back to this PrefHashFilter.
  FilterOnLoadInterceptor filter_on_load_interceptor_;

  // A map of paths to TrackedPreferences; this map owns this individual
  // TrackedPreference objects.
  using TrackedPreferencesMap =
      std::unordered_map<std::string, std::unique_ptr<TrackedPreference>>;

  // A map from changed paths to their corresponding TrackedPreferences (which
  // aren't owned by this map).
  using ChangedPathsMap = std::map<std::string, const TrackedPreference*>;

  std::unique_ptr<PrefHashStore> pref_hash_store_;

  // A store and contents on which to perform extra validations without
  // triggering resets.
  // Will be null if the platform does not support external validation.
  base::Optional<StoreContentsPair> external_validation_hash_store_pair_;

  // Notified if a reset occurs in a call to FilterOnLoad.
  mojo::Remote<prefs::mojom::ResetOnLoadObserver> reset_on_load_observer_;

  TrackedPreferencesMap tracked_paths_;

  // The set of all paths whose value has changed since the last call to
  // FilterSerializeData.
  ChangedPathsMap changed_paths_;

  DISALLOW_COPY_AND_ASSIGN(PrefHashFilter);
};

#endif  // SERVICES_PREFERENCES_TRACKED_PREF_HASH_FILTER_H_
