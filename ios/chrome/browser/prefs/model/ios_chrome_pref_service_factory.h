// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_MODEL_IOS_CHROME_PREF_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PREFS_MODEL_IOS_CHROME_PREF_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"

class PrefRegistry;
class PrefService;
class PrefStore;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace policy {
class BrowserPolicyConnector;
class PolicyService;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Factory methods that create and initialize a new instance of a PrefService
// for Chrome on iOS with the applicable PrefStores. The `pref_filename` points
// to the user preference file. This is the usual way to create a new
// PrefService. `pref_registry` keeps the list of registered prefs and their
// default values.
std::unique_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<PrefRegistry>& pref_registry,
    policy::PolicyService* policy_service,
    policy::BrowserPolicyConnector* policy_connector);

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreateProfilePrefs(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    policy::PolicyService* policy_service,
    policy::BrowserPolicyConnector* policy_connector,
    const scoped_refptr<PrefStore>& supervised_user_prefs,
    bool async);

// Creates an incognito copy of `pref_service` that shares most prefs but uses
// a fresh non-persistent overlay for the user pref store.
std::unique_ptr<sync_preferences::PrefServiceSyncable>
CreateIncognitoProfilePrefs(
    sync_preferences::PrefServiceSyncable* main_pref_store);

#endif  // IOS_CHROME_BROWSER_PREFS_MODEL_IOS_CHROME_PREF_SERVICE_FACTORY_H_
