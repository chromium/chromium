// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "url/origin.h"

namespace device {

#if defined(OS_WIN)
const base::Feature kWebAuthUseNativeWinApi{"WebAuthenticationUseNativeWinApi",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_WIN)

extern const base::Feature kWebAuthBiometricEnrollment{
    "WebAuthenticationBiometricEnrollment", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kWebAuthPhoneSupport{
    "WebAuthenticationPhoneSupport", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kWebAuthGetAssertionFeaturePolicy{
    "WebAuthenticationGetAssertionFeaturePolicy",
    base::FEATURE_ENABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
const base::Feature kWebAuthCableLowLatency{"WebAuthenticationCableLowLatency",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS) || defined(OS_LINUX)

#if BUILDFLAG(IS_ASH)
const base::Feature kWebAuthCrosPlatformAuthenticator{
    "WebAuthenticationCrosPlatformAuthenticator",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ASH)

extern const base::Feature kWebAuthAttestationBlockList{
    "WebAuthentiationAttestationBlockList", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::FeatureParam<std::string> kWebAuthAttestationBlockedDomains{
    &kWebAuthAttestationBlockList,
    "domains",
    "",
};

bool DoesMatchWebAuthAttestationBlockedDomains(const url::Origin& origin) {
  const std::string& blocked_domains = kWebAuthAttestationBlockedDomains.Get();
  if (blocked_domains.empty()) {
    return false;
  }

  const std::vector<std::string> domains = base::SplitString(
      blocked_domains, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& domain : domains) {
    static constexpr char kWildcardPrefix[] = "(*.)";
    if (!domain.empty() && domain[0] == '(' &&
        domain.find(kWildcardPrefix) == 0) {
      base::StringPiece domain_part(domain);
      domain_part.remove_prefix(sizeof(kWildcardPrefix) - 1);
      if (origin.DomainIs(domain_part)) {
        return true;
      }
    } else if (!origin.opaque() && origin.host() == domain) {
      return true;
    }
  }

  return false;
}

}  // namespace device
