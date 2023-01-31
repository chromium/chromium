// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/policy/headless_policies.h"

#include "components/headless/policy/headless_mode_prefs.h"
#include "components/policy/core/browser/url_blocklist_manager.h"  // nogncheck http://crbug.com/1227148
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "headless/lib/browser/policy/headless_prefs.h"

namespace headless {

void RegisterHeadlessPrefs(user_prefs::PrefRegistrySyncable* registry) {
  DCHECK(registry);
  headless::RegisterPrefs(registry);
  registry->RegisterBooleanPref(
      headless::prefs::kDevToolsRemoteDebuggingAllowed, true);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
}

bool IsRemoteDebuggingAllowed(const PrefService* pref_service) {
  if (!pref_service)
    return true;

  return pref_service->GetBoolean(
      headless::prefs::kDevToolsRemoteDebuggingAllowed);
}

}  // namespace headless
