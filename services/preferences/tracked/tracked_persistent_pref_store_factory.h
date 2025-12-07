// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_TRACKED_PERSISTENT_PREF_STORE_FACTORY_H_
#define SERVICES_PREFERENCES_TRACKED_TRACKED_PERSISTENT_PREF_STORE_FACTORY_H_

#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "services/preferences/public/mojom/preferences.mojom.h"

class PersistentPrefStore;
namespace os_crypt_async {
class OSCryptAsync;
}

PersistentPrefStore* CreateTrackedPersistentPrefStore(
    prefs::mojom::TrackedPersistentPrefStoreConfigurationPtr config,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    os_crypt_async::OSCryptAsync* os_crypt);

// TODO(sammc): This should move somewhere more appropriate in the longer term.
void InitializeMasterPrefsTracking(
    prefs::mojom::TrackedPersistentPrefStoreConfigurationPtr configuration,
    base::Value::Dict& master_prefs,
    os_crypt_async::OSCryptAsync* os_crypt);

#endif  // SERVICES_PREFERENCES_TRACKED_TRACKED_PERSISTENT_PREF_STORE_FACTORY_H_
