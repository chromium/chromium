// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_
#define NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_

#include "base/containers/enum_set.h"

namespace net {

// An enum of possible overrides for cookie setting checks.
// Use CookieSettingOverrides below for specifying any number of overrides
// together.
enum class CookieSettingOverride {
  kNone = 0,
  // When specified, the user has indicated to force allowing third-party
  // cookies.
  kForceThirdPartyByUser = 1,
};

using CookieSettingOverrides =
    base::EnumSet<CookieSettingOverride,
                  CookieSettingOverride::kNone,
                  CookieSettingOverride::kForceThirdPartyByUser>;

}  // namespace net

#endif  // NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_
