// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/scoped_pref_connection_builder.h"

#include "services/preferences/persistent_pref_store_impl.h"
#include "services/preferences/pref_store_impl.h"

namespace prefs {

ScopedPrefConnectionBuilder::ScopedPrefConnectionBuilder(
    std::vector<std::string> observed_prefs,
    mojom::PrefStoreConnector::ConnectCallback callback)
    : callback_(std::move(callback)),
      observed_prefs_(std::move(observed_prefs)) {}

void ScopedPrefConnectionBuilder::ProvidePrefStoreConnections(
    const base::flat_map<PrefValueStore::PrefStoreType,
                         std::unique_ptr<PrefStoreImpl>>& pref_stores) {
  for (const auto& pref_store : pref_stores) {
    ProvidePrefStoreConnection(pref_store.first, pref_store.second.get());
  }
}

void ScopedPrefConnectionBuilder::ProvidePrefStoreConnection(
    PrefValueStore::PrefStoreType type,
    PrefStoreImpl* pref_store) {
  bool inserted =
      connections_.emplace(type, pref_store->AddObserver(observed_prefs_))
          .second;
  DCHECK(inserted);
}

void ScopedPrefConnectionBuilder::ProvidePersistentPrefStore(
    PersistentPrefStoreImpl* persistent_pref_store) {
  DCHECK(!persistent_pref_store_connection_);
  persistent_pref_store_connection_ = persistent_pref_store->CreateConnection(
      PersistentPrefStoreImpl::ObservedPrefs(observed_prefs_.begin(),
                                             observed_prefs_.end()));
}

void ScopedPrefConnectionBuilder::ProvideIncognitoPersistentPrefStoreUnderlay(
    PersistentPrefStoreImpl* persistent_pref_store,
    const std::vector<const char*>& persistent_perf_names) {
  PersistentPrefStoreImpl::ObservedPrefs observed_prefs(observed_prefs_.begin(),
                                                        observed_prefs_.end());
  std::vector<std::string> filtered_persistent_perf_names;
  for (const char* persistent_perf_name : persistent_perf_names) {
    if (base::Contains(observed_prefs, persistent_perf_name))
      filtered_persistent_perf_names.emplace_back(persistent_perf_name);
  }
  incognito_connection_ = mojom::IncognitoPersistentPrefStoreConnection::New(
      persistent_pref_store->CreateConnection(std::move(observed_prefs)),
      std::move(filtered_persistent_perf_names));
}

void ScopedPrefConnectionBuilder::ProvideDefaults(
    std::vector<mojom::PrefRegistrationPtr> defaults) {
  defaults_ = std::move(defaults);
}

ScopedPrefConnectionBuilder::~ScopedPrefConnectionBuilder() {
  std::move(callback_).Run(std::move(persistent_pref_store_connection_),
                           std::move(incognito_connection_),
                           std::move(defaults_), std::move(connections_));
}

}  // namespace prefs
