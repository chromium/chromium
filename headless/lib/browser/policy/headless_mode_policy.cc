// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/policy/headless_mode_policy.h"

#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "headless/lib/browser/headless_pref_names.h"

namespace policy {

// static
void HeadlessModePolicy::RegisterLocalPrefs(PrefRegistrySimple* registry) {
#if defined(HEADLESS_MODE_POLICY_SUPPORTED)
  registry->RegisterIntegerPref(headless::prefs::kHeadlessMode,
                                static_cast<int>(HeadlessMode::kDefaultValue));
#endif
}

// static
HeadlessModePolicy::HeadlessMode HeadlessModePolicy::GetPolicy(
    const PrefService* pref_service) {
  // If preferences are not available (happens in tests), assume the default
  // value.
  if (!pref_service)
    return HeadlessMode::kDefaultValue;

#if defined(HEADLESS_MODE_POLICY_SUPPORTED)
  int value = pref_service->GetInteger(headless::prefs::kHeadlessMode);
  if (value < static_cast<int>(HeadlessMode::kMinValue) ||
      value > static_cast<int>(HeadlessMode::kMaxValue)) {
    // This should never happen, because the |kHeadlessMode| pref is
    // only set by HeadlessModePolicy which validates the value range.
    // If it is not set, it will have its default value which is also valid,
    // see |RegisterProfilePrefs|.
    NOTREACHED();
    return HeadlessMode::kDefaultValue;
  }

  return static_cast<HeadlessMode>(value);
#else
  return HeadlessMode::kDefaultValue;
#endif
}

// static
bool HeadlessModePolicy::IsHeadlessDisabled(const PrefService* pref_service) {
  return GetPolicy(pref_service) != HeadlessMode::kEnabled;
}

#if defined(HEADLESS_MODE_POLICY_SUPPORTED)
HeadlessModePolicyHandler::HeadlessModePolicyHandler()
    : IntRangePolicyHandler(
          key::kHeadlessMode,
          headless::prefs::kHeadlessMode,
          static_cast<int>(HeadlessModePolicy::HeadlessMode::kMinValue),
          static_cast<int>(HeadlessModePolicy::HeadlessMode::kMaxValue),
          false) {}

HeadlessModePolicyHandler::~HeadlessModePolicyHandler() = default;
#endif

}  // namespace policy
