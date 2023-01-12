// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/tracked_persistent_pref_store_factory.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/pref_name_set.h"
#include "components/prefs/segregated_pref_store.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"
#include "services/preferences/tracked/pref_hash_filter.h"
#include "services/preferences/tracked/pref_hash_store_impl.h"
#include "services/preferences/tracked/temp_scoped_dir_cleaner.h"
#include "services/preferences/tracked/tracked_preferences_migration.h"

#if BUILDFLAG(IS_WIN)
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "services/preferences/tracked/registry_hash_store_contents_win.h"
#endif

namespace {

void RemoveValueSilently(const base::WeakPtr<JsonPrefStore> pref_store,
                         const std::string& key) {
  if (pref_store) {
    pref_store->RemoveValueSilently(
        key, WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  }
}

std::unique_ptr<PrefHashStore> CreatePrefHashStore(
    const prefs::mojom::TrackedPersistentPrefStoreConfiguration& config,
    bool use_super_mac) {
  return std::make_unique<PrefHashStoreImpl>(
      config.seed, config.legacy_device_id, use_super_mac);
}

std::pair<std::unique_ptr<PrefHashStore>, std::unique_ptr<HashStoreContents>>
GetExternalVerificationPrefHashStorePair(
    const prefs::mojom::TrackedPersistentPrefStoreConfiguration& config,
    scoped_refptr<TempScopedDirCleaner> temp_dir_cleaner) {
#if BUILDFLAG(IS_WIN)
  return std::make_pair(
      std::make_unique<PrefHashStoreImpl>(config.registry_seed,
                                          config.legacy_device_id,
                                          false /* use_super_mac */),
      std::make_unique<RegistryHashStoreContentsWin>(
          base::AsWString(config.registry_path),
          config.unprotected_pref_filename.DirName().BaseName().value(),
          std::move(temp_dir_cleaner)));
#else
  return std::make_pair(nullptr, nullptr);
#endif
}

}  // namespace

PersistentPrefStore* CreateTrackedPersistentPrefStore(
    prefs::mojom::TrackedPersistentPrefStoreConfigurationPtr config,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
      unprotected_configuration;
  std::vector<prefs::mojom::TrackedPreferenceMetadataPtr>
      protected_configuration;
  PrefNameSet protected_pref_names;
  PrefNameSet unprotected_pref_names;
  for (auto& metadata : config->tracking_configuration) {
    if (metadata->enforcement_level > prefs::mojom::TrackedPreferenceMetadata::
                                          EnforcementLevel::NO_ENFORCEMENT) {
      protected_pref_names.insert(metadata->name);
      protected_configuration.push_back(std::move(metadata));
    } else {
      unprotected_pref_names.insert(metadata->name);
      unprotected_configuration.push_back(std::move(metadata));
    }
  }
  config->tracking_configuration.clear();

  scoped_refptr<TempScopedDirCleaner> temp_scoped_dir_cleaner;
#if BUILDFLAG(IS_WIN)
  // For tests that create a profile in a ScopedTempDir, share a ref_counted
  // object between the unprotected and protected hash filter's
  // RegistryHashStoreContentsWin which will clear the registry keys when
  // destroyed. (https://crbug.com/721245)
  if (base::StartsWith(
          config->unprotected_pref_filename.DirName().BaseName().value(),
          base::ScopedTempDir::GetTempDirPrefix(),
          base::CompareCase::INSENSITIVE_ASCII)) {
    temp_scoped_dir_cleaner =
        base::MakeRefCounted<TempScopedDirRegistryCleaner>();
  }
#endif

  mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>
      validation_delegate;
  validation_delegate.Bind(std::move(config->validation_delegate));
  auto validation_delegate_ref = base::MakeRefCounted<base::RefCountedData<
      mojo::Remote<prefs::mojom::TrackedPreferenceValidationDelegate>>>(
      std::move(validation_delegate));
  std::unique_ptr<PrefHashFilter> unprotected_pref_hash_filter(
      new PrefHashFilter(CreatePrefHashStore(*config, false),
                         GetExternalVerificationPrefHashStorePair(
                             *config, temp_scoped_dir_cleaner),
                         unprotected_configuration, mojo::NullRemote(),
                         validation_delegate_ref, config->reporting_ids_count));
  std::unique_ptr<PrefHashFilter> protected_pref_hash_filter(new PrefHashFilter(
      CreatePrefHashStore(*config, true),
      GetExternalVerificationPrefHashStorePair(*config,
                                               temp_scoped_dir_cleaner),
      protected_configuration, std::move(config->reset_on_load_observer),
      validation_delegate_ref, config->reporting_ids_count));

  PrefHashFilter* raw_unprotected_pref_hash_filter =
      unprotected_pref_hash_filter.get();
  PrefHashFilter* raw_protected_pref_hash_filter =
      protected_pref_hash_filter.get();

  scoped_refptr<JsonPrefStore> unprotected_pref_store(new JsonPrefStore(
      config->unprotected_pref_filename,
      std::move(unprotected_pref_hash_filter), io_task_runner.get()));
  scoped_refptr<JsonPrefStore> protected_pref_store(new JsonPrefStore(
      config->protected_pref_filename, std::move(protected_pref_hash_filter),
      io_task_runner.get()));

  SetupTrackedPreferencesMigration(
      unprotected_pref_names, protected_pref_names,
      base::BindRepeating(&RemoveValueSilently,
                          unprotected_pref_store->AsWeakPtr()),
      base::BindRepeating(&RemoveValueSilently,
                          protected_pref_store->AsWeakPtr()),
      base::BindRepeating(&JsonPrefStore::RegisterOnNextSuccessfulWriteReply,
                          unprotected_pref_store->AsWeakPtr()),
      base::BindRepeating(&JsonPrefStore::RegisterOnNextSuccessfulWriteReply,
                          protected_pref_store->AsWeakPtr()),
      CreatePrefHashStore(*config, false), CreatePrefHashStore(*config, true),
      raw_unprotected_pref_hash_filter, raw_protected_pref_hash_filter);

  return new SegregatedPrefStore(std::move(unprotected_pref_store),
                                 std::move(protected_pref_store),
                                 std::move(protected_pref_names));
}

void InitializeMasterPrefsTracking(
    prefs::mojom::TrackedPersistentPrefStoreConfigurationPtr configuration,
    base::Value::Dict& master_prefs) {
  PrefHashFilter(
      CreatePrefHashStore(*configuration, false),
      GetExternalVerificationPrefHashStorePair(*configuration, nullptr),
      configuration->tracking_configuration, mojo::NullRemote(), nullptr,
      configuration->reporting_ids_count)
      .Initialize(master_prefs);
}
