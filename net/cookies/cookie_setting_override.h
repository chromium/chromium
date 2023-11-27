// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_
#define NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_

#include "base/containers/enum_set.h"

namespace net {

// An enum of possible overrides for cookie setting checks.
// Use CookieSettingOverrides below for specifying any number of overrides
// together. The notion of no overrides is conveyable via an empty set.
enum class CookieSettingOverride {
  kMinValue = 0,
  // When specified, third-party cookies may be allowed based on existence of
  // TopLevelStorageAccess grants.
  kTopLevelStorageAccessGrantEligible = kMinValue,
  // When present, the caller may use an existing Storage Access API grant (if
  // a matching grant exists) to access third-party cookies. Otherwise, Storage
  // Access API grants do not apply.
  kStorageAccessGrantEligible = 1,
  // Allows TPCD mitigations to be skipped when checking if third party cookies
  // are allowed, meaning cookies will be blocked despite the presence of any of
  // these grants/heuristics.
  kSkipTPCDHeuristicsGrant = 2,
  // Corresponds to skipping checks on the TPCD_SUPPORT content setting, which
  // backs 3PC accesses granted via 3PC deprecation trial.
  kSkipTPCDSupport = 3,
  kSkipTPCDMetadataGrant = 4,

  kMaxValue = kSkipTPCDMetadataGrant,
};

using CookieSettingOverrides = base::EnumSet<CookieSettingOverride,
                                             CookieSettingOverride::kMinValue,
                                             CookieSettingOverride::kMaxValue>;

}  // namespace net

#endif  // NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_
