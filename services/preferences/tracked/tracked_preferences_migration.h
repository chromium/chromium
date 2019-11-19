// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_
#define SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "services/preferences/tracked/pref_hash_store.h"

class InterceptablePrefFilter;
class PrefHashStore;

// Sets up InterceptablePrefFilter::FilterOnLoadInterceptors on
// |unprotected_pref_filter| and |protected_pref_filter| which prevents each
// filter from running their on load operations until the interceptors decide to
// hand the prefs back to them (after migration is complete). |
// (un)protected_store_cleaner| and
// |register_on_successful_(un)protected_store_write_callback| are used to do
// post-migration cleanup tasks. Those should be bound to weak pointers to avoid
// blocking shutdown. |(un)protected_pref_hash_store| is used to migrate MACs
// along with their protected preferences. Migrated MACs will only be cleared
// from their old location in a subsequent run. The migration framework is
// resilient to a failed cleanup (it will simply try again in the next Chrome
// run).
void SetupTrackedPreferencesMigration(
    const std::set<std::string>& unprotected_pref_names,
    const std::set<std::string>& protected_pref_names,
    const base::Callback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::Callback<void(const std::string& key)>& protected_store_cleaner,
    const base::Callback<void(base::OnceClosure)>&
        register_on_successful_unprotected_store_write_callback,
    const base::Callback<void(base::OnceClosure)>&
        register_on_successful_protected_store_write_callback,
    std::unique_ptr<PrefHashStore> unprotected_pref_hash_store,
    std::unique_ptr<PrefHashStore> protected_pref_hash_store,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter);

#endif  // SERVICES_PREFERENCES_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_
