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
  // a matching grant exists) to access third-party cookies. This "opt-in"
  // signal is from script execution, i.e. `document.requestStorageAccess()`.
  kStorageAccessGrantEligible = 1,
  // kSkipTPCDHeuristicsGrant = 2,  // Deprecated
  // kSkipTPCDMetadataGrant = 3,  // Deprecated
  // kSkipTPCDTrial = 4,  // Deprecated
  // kSkipTopLevelTPCDTrial = 5,  // Deprecated
  // When specified, third party cookies should be forced disabled.
  // Other cookie exceptions like the storage access API could result in
  // third party cookies still being used when this is forced disabled. This
  // override takes precedence over `kForceEnableThirdPartyCookies`.
  kForceDisableThirdPartyCookies = 6,
  // When present, the caller may use an existing Storage Access API grant to
  // access third-party cookies. Note that some integrations which have more
  // stringent requirements, such as the FedCM/SAA integration (which requires
  // the `identity-credentials-get` policy), are not in scope for this variant.
  kStorageAccessGrantEligibleViaHeader = 7,
  // kForceEnableThirdPartyCookieMitigations = 8,  // Deprecated
  // When present, the context is sandboxed in a frame that is same-site
  // with the top-level up its entire ancestor chain. SameSite=None
  // cookies should be included in same-site requests from sandboxed contexts
  // that have the 'allow-same-site-none-cookies' value.
  kAllowSameSiteNoneCookiesInSandbox = 9,
  // When specified, third-party cookies should behave as they would when no
  // setting or OT exists to restrict them. This override is secondary to
  // `kForceDisableThirdPartyCookies` and will not have any effect if both
  // exist.
  kForceEnableThirdPartyCookies = 10,

  kMaxValue = kForceEnableThirdPartyCookies,
};

using CookieSettingOverrides = base::EnumSet<CookieSettingOverride,
                                             CookieSettingOverride::kMinValue,
                                             CookieSettingOverride::kMaxValue>;

}  // namespace net

#endif  // NET_COOKIES_COOKIE_SETTING_OVERRIDE_H_
