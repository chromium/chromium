// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_POLICY_HEADLESS_POLICIES_H_
#define HEADLESS_LIB_BROWSER_POLICY_HEADLESS_POLICIES_H_

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace headless {

// Registers headless policies' prefs in |registry|.
void RegisterHeadlessPrefs(user_prefs::PrefRegistrySyncable* registry);

// Returns positive if current policy in |pref_service| allows Remote Debugging.
bool IsRemoteDebuggingAllowed(const PrefService* pref_service);

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_POLICY_HEADLESS_POLICIES_H_
