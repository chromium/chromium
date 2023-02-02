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
  // When specified, the user has indicated to force allowing third-party
  // cookies.
  kForceThirdPartyByUser = 0,
  // When specified, third-party cookies may be allowed based on existence of
  // TopLevelStorageAccess grants.
  kTopLevelStorageAccessGrantEligible = 1,
  // When present, the caller may use an existing Storage Access API grant (if
  // a matching grant exists) to access third-party cookies. Otherwise, Storage
  // Access API grants do not apply.
  // TODO(https://crbug.com/1401089): this description isn't true yet; these
  // variants are currently ignored, and grants are always accessible. This will
  // be updated once all callers have been updated to pass this variant when
  // appropriate.
  kStorageAccessGrantEligible = 2,
  kMaxValue = kStorageAccessGrantEligible,
};

using CookieSettingOverrides =
    base::EnumSet<CookieSettingOverride,
                  CookieSettingOverride::kForceThirdPartyByUser,
                  CookieSettingOverride::kMaxValue>;

}  // namespace net

#endif  // NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_
