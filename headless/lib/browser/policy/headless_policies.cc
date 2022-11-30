// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/policy/headless_policies.h"

#include "base/check.h"
#include "components/policy/core/browser/url_blocklist_manager.h"  // nogncheck http://crbug.com/1227148
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "headless/lib/browser/headless_pref_names.h"
#include "headless/lib/browser/policy/headless_mode_policy.h"

namespace policy {

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  DCHECK(registry);
  registry->RegisterBooleanPref(
      headless::prefs::kDevToolsRemoteDebuggingAllowed, true);
  HeadlessModePolicy::RegisterLocalPrefs(registry);
  URLBlocklistManager::RegisterProfilePrefs(registry);
}

bool IsRemoteDebuggingAllowed(const PrefService* pref_service) {
  // If preferences are not available, assume the default value.
  if (!pref_service)
    return true;

  return pref_service->GetBoolean(
      headless::prefs::kDevToolsRemoteDebuggingAllowed);
}

}  // namespace policy
