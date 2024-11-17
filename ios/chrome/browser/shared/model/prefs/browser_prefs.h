// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_BROWSER_PREFS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_BROWSER_PREFS_H_

class PrefRegistrySimple;
class PrefService;

namespace base {
class FilePath;
}
namespace user_prefs {
class PrefRegistrySyncable;
}

// Registers all prefs that will be used via the local state PrefService.
void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

// Registers all prefs that will be used via a PrefService attached to a
// profile.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Migrate/cleanup deprecated prefs in local state. Over time, long deprecated
// prefs should be removed as new ones are added, but this call should never go
// away (even if it becomes an empty call for some time) as it should remain
// *the* place to drop deprecated local state prefs at.
void MigrateObsoleteLocalStatePrefs(PrefService* prefs);

// Migrate/cleanup deprecated prefs in the profile's pref store. Over
// time, long deprecated prefs should be removed as new ones are added, but this
// call should never go away (even if it becomes an empty call for some time) as
// it should remain *the* place to drop deprecated profile's prefs at.
void MigrateObsoleteProfilePrefs(const base::FilePath& state_path,
                                 PrefService* prefs);

// Migrate/cleanup deprecated prefs from the standard NSUserDefault store. Over
// time, long deprecated prefs should be removed as new ones are added, but this
// call should never go away (even if it becomes an empty call for some time) as
// it should remain *the* place to drop deprecated NSUserDefault at.
void MigrateObsoleteUserDefault();

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_PREFS_BROWSER_PREFS_H_
