// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_inclusion_status.h"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/enum_set.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace net {

using ExclusionReason = CookieInclusionStatus::ExclusionReason;
using WarningReason = CookieInclusionStatus::WarningReason;

CookieInclusionStatus::CookieInclusionStatus() = default;

CookieInclusionStatus::CookieInclusionStatus(
    const CookieInclusionStatus& other) = default;

CookieInclusionStatus& CookieInclusionStatus::operator=(
    const CookieInclusionStatus& other) = default;

bool CookieInclusionStatus::operator==(
    const CookieInclusionStatus& other) const = default;

bool CookieInclusionStatus::IsInclude() const {
  return exclusion_reasons_.empty();
}

bool CookieInclusionStatus::HasExclusionReason(ExclusionReason reason) const {
  return exclusion_reasons_.Has(reason);
}

bool CookieInclusionStatus::HasOnlyExclusionReason(
    ExclusionReason reason) const {
  return exclusion_reasons_.Has(reason) && exclusion_reasons_.size() == 1;
}

void CookieInclusionStatus::AddExclusionReason(ExclusionReason reason) {
  exclusion_reasons_.Put(reason);
  // If the cookie would be excluded for reasons other than the new SameSite
  // rules, don't bother warning about it.
  MaybeClearSameSiteWarning();
  // If the cookie would be excluded for reasons unrelated to 3pcd, don't bother
  // warning about 3pcd.
  MaybeClearThirdPartyPhaseoutReason();
  // If the cookie would have been excluded, clear the exemption reason.
  exemption_reason_ = ExemptionReason::kNone;
}

void CookieInclusionStatus::RemoveExclusionReason(ExclusionReason reason) {
  exclusion_reasons_.Remove(reason);
}

void CookieInclusionStatus::RemoveExclusionReasons(
    ExclusionReasonBitset reasons) {
  exclusion_reasons_ = ExclusionReasonsWithout(reasons);
}

void CookieInclusionStatus::MaybeSetExemptionReason(ExemptionReason reason) {
  if (IsInclude() && exemption_reason_ == ExemptionReason::kNone) {
    exemption_reason_ = reason;
  }
}

CookieInclusionStatus::ExclusionReasonBitset
CookieInclusionStatus::ExclusionReasonsWithout(
    ExclusionReasonBitset reasons) const {
  CookieInclusionStatus::ExclusionReasonBitset result(exclusion_reasons_);
  result.RemoveAll(reasons);
  return result;
}

void CookieInclusionStatus::MaybeClearSameSiteWarning() {
  if (!ExclusionReasonsWithout(
           {
               ExclusionReason::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
               ExclusionReason::EXCLUDE_SAMESITE_NONE_INSECURE,
           })
           .empty()) {
    RemoveWarningReason(
        WarningReason::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
    RemoveWarningReason(WarningReason::WARN_SAMESITE_NONE_INSECURE);
    RemoveWarningReason(
        WarningReason::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);
  }

  if (!ShouldRecordDowngradeMetrics()) {
    RemoveWarningReason(
        WarningReason::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(
        WarningReason::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(
        WarningReason::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE);
    RemoveWarningReason(
        WarningReason::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(WarningReason::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE);

    RemoveWarningReason(
        WarningReason::WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION);
  }
}

void CookieInclusionStatus::MaybeClearThirdPartyPhaseoutReason() {
  if (!IsInclude()) {
    RemoveWarningReason(WarningReason::WARN_THIRD_PARTY_PHASEOUT);
  }
  if (!ExclusionReasonsWithout(
           {ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT,
            ExclusionReason::
                EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET})
           .empty()) {
    RemoveExclusionReasons(
        {ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT,
         ExclusionReason::EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET});
  }
}

bool CookieInclusionStatus::ShouldRecordDowngradeMetrics() const {
  return ExclusionReasonsWithout(
             {
                 ExclusionReason::EXCLUDE_SAMESITE_STRICT,
                 ExclusionReason::EXCLUDE_SAMESITE_LAX,
                 ExclusionReason::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
             })
      .empty();
}

bool CookieInclusionStatus::ShouldWarn() const {
  return !warning_reasons_.empty();
}

bool CookieInclusionStatus::HasWarningReason(WarningReason reason) const {
  return warning_reasons_.Has(reason);
}

bool CookieInclusionStatus::HasSchemefulDowngradeWarning(
    WarningReason* reason) const {
  if (!ShouldWarn())
    return false;

  const WarningReason kDowngradeWarnings[] = {
      WarningReason::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE,
      WarningReason::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE,
      WarningReason::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE,
      WarningReason::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE,
      WarningReason::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE,
  };

  for (auto warning : kDowngradeWarnings) {
    if (!HasWarningReason(warning))
      continue;

    if (reason)
      *reason = warning;

    return true;
  }

  return false;
}

void CookieInclusionStatus::AddWarningReason(WarningReason reason) {
  warning_reasons_.Put(reason);
}

void CookieInclusionStatus::RemoveWarningReason(WarningReason reason) {
  warning_reasons_.Remove(reason);
}

CookieInclusionStatus::ContextDowngradeMetricValues
CookieInclusionStatus::GetBreakingDowngradeMetricsEnumValue(
    const GURL& url) const {
  bool url_is_secure = url.SchemeIsCryptographic();

  // Start the |reason| as something other than the downgrade warnings.
  WarningReason reason = WarningReason::MAX_WARNING_REASON;

  // Don't bother checking the return value because the default switch case
  // will handle if no reason was found.
  HasSchemefulDowngradeWarning(&reason);

  switch (reason) {
    case WarningReason::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::kStrictLaxStrictSecure
                 : ContextDowngradeMetricValues::kStrictLaxStrictInsecure;
    case WarningReason::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::kStrictCrossStrictSecure
                 : ContextDowngradeMetricValues::kStrictCrossStrictInsecure;
    case WarningReason::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::kStrictCrossLaxSecure
                 : ContextDowngradeMetricValues::kStrictCrossLaxInsecure;
    case WarningReason::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::kLaxCrossStrictSecure
                 : ContextDowngradeMetricValues::kLaxCrossStrictInsecure;
    case WarningReason::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE:
      return url_is_secure ? ContextDowngradeMetricValues::kLaxCrossLaxSecure
                           : ContextDowngradeMetricValues::kLaxCrossLaxInsecure;
    default:
      return url_is_secure ? ContextDowngradeMetricValues::kNoDowngradeSecure
                           : ContextDowngradeMetricValues::kNoDowngradeInsecure;
  }
}

std::string CookieInclusionStatus::GetDebugString() const {
  std::string out;

  if (IsInclude())
    base::StrAppend(&out, {"INCLUDE, "});

  constexpr std::pair<ExclusionReason, const char*> exclusion_reasons[] = {
      {ExclusionReason::EXCLUDE_UNKNOWN_ERROR, "EXCLUDE_UNKNOWN_ERROR"},
      {ExclusionReason::EXCLUDE_HTTP_ONLY, "EXCLUDE_HTTP_ONLY"},
      {ExclusionReason::EXCLUDE_SECURE_ONLY, "EXCLUDE_SECURE_ONLY"},
      {ExclusionReason::EXCLUDE_DOMAIN_MISMATCH, "EXCLUDE_DOMAIN_MISMATCH"},
      {ExclusionReason::EXCLUDE_NOT_ON_PATH, "EXCLUDE_NOT_ON_PATH"},
      {ExclusionReason::EXCLUDE_SAMESITE_STRICT, "EXCLUDE_SAMESITE_STRICT"},
      {ExclusionReason::EXCLUDE_SAMESITE_LAX, "EXCLUDE_SAMESITE_LAX"},
      {ExclusionReason::EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
       "EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX"},
      {ExclusionReason::EXCLUDE_SAMESITE_NONE_INSECURE,
       "EXCLUDE_SAMESITE_NONE_INSECURE"},
      {ExclusionReason::EXCLUDE_USER_PREFERENCES, "EXCLUDE_USER_PREFERENCES"},
      {ExclusionReason::EXCLUDE_FAILURE_TO_STORE, "EXCLUDE_FAILURE_TO_STORE"},
      {ExclusionReason::EXCLUDE_NONCOOKIEABLE_SCHEME,
       "EXCLUDE_NONCOOKIEABLE_SCHEME"},
      {ExclusionReason::EXCLUDE_OVERWRITE_SECURE, "EXCLUDE_OVERWRITE_SECURE"},
      {ExclusionReason::EXCLUDE_OVERWRITE_HTTP_ONLY,
       "EXCLUDE_OVERWRITE_HTTP_ONLY"},
      {ExclusionReason::EXCLUDE_INVALID_DOMAIN, "EXCLUDE_INVALID_DOMAIN"},
      {ExclusionReason::EXCLUDE_INVALID_PREFIX, "EXCLUDE_INVALID_PREFIX"},
      {ExclusionReason::EXCLUDE_INVALID_PARTITIONED,
       "EXCLUDE_INVALID_PARTITIONED"},
      {ExclusionReason::EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE,
       "EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE"},
      {ExclusionReason::EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE,
       "EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE"},
      {ExclusionReason::EXCLUDE_DOMAIN_NON_ASCII, "EXCLUDE_DOMAIN_NON_ASCII"},
      {ExclusionReason::EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
       "EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET"},
      {ExclusionReason::EXCLUDE_PORT_MISMATCH, "EXCLUDE_PORT_MISMATCH"},
      {ExclusionReason::EXCLUDE_SCHEME_MISMATCH, "EXCLUDE_SCHEME_MISMATCH"},
      {ExclusionReason::EXCLUDE_SHADOWING_DOMAIN, "EXCLUDE_SHADOWING_DOMAIN"},
      {ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER,
       "EXCLUDE_DISALLOWED_CHARACTER"},
      {ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT,
       "EXCLUDE_THIRD_PARTY_PHASEOUT"},
      {ExclusionReason::EXCLUDE_NO_COOKIE_CONTENT, "EXCLUDE_NO_COOKIE_CONTENT"},
      {ExclusionReason::EXCLUDE_ANONYMOUS_CONTEXT, "EXCLUDE_ANONYMOUS_CONTEXT"},
  };
  static_assert(
      std::size(exclusion_reasons) == ExclusionReasonBitset::kValueCount,
      "Please ensure all ExclusionReason variants are enumerated in "
      "GetDebugString");
  static_assert(std::ranges::is_sorted(exclusion_reasons),
                "Please keep the ExclusionReason variants sorted in numerical "
                "order in GetDebugString");

  for (const auto& reason : exclusion_reasons) {
    if (HasExclusionReason(reason.first))
      base::StrAppend(&out, {reason.second, ", "});
  }

  // Add warning
  if (!ShouldWarn()) {
    base::StrAppend(&out, {"DO_NOT_WARN, "});
  }

  constexpr std::pair<WarningReason, const char*> warning_reasons[] = {
      {WarningReason::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
       "WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT"},
      {WarningReason::WARN_SAMESITE_NONE_INSECURE,
       "WARN_SAMESITE_NONE_INSECURE"},
      {WarningReason::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE,
       "WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE"},
      {WarningReason::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE,
       "WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE"},
      {WarningReason::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE,
       "WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE"},
      {WarningReason::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE,
       "WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE"},
      {WarningReason::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE,
       "WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE"},
      {WarningReason::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE,
       "WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE"},
      {WarningReason::WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC,
       "WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC"},
      {WarningReason::WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION,
       "WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION"},
      {WarningReason::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE,
       "WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE"},
      {WarningReason::WARN_DOMAIN_NON_ASCII, "WARN_DOMAIN_NON_ASCII"},
      {WarningReason::WARN_PORT_MISMATCH, "WARN_PORT_MISMATCH"},
      {WarningReason::WARN_SCHEME_MISMATCH, "WARN_SCHEME_MISMATCH"},
      {WarningReason::WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME,
       "WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME"},
      {WarningReason::WARN_SHADOWING_DOMAIN, "WARN_SHADOWING_DOMAIN"},
      {WarningReason::WARN_THIRD_PARTY_PHASEOUT, "WARN_THIRD_PARTY_PHASEOUT"},
  };
  static_assert(std::size(warning_reasons) == WarningReasonBitset::kValueCount,
                "Please ensure all WarningReason variants are enumerated in "
                "GetDebugString");
  static_assert(std::ranges::is_sorted(warning_reasons),
                "Please keep the WarningReason variants sorted in numerical "
                "order in GetDebugString");

  for (const auto& reason : warning_reasons) {
    if (HasWarningReason(reason.first))
      base::StrAppend(&out, {reason.second, ", "});
  }

  // Add exemption reason
  std::string_view reason;
  switch (exemption_reason()) {
    case ExemptionReason::kNone:
      reason = "NO_EXEMPTION";
      break;
    case ExemptionReason::kUserSetting:
      reason = "ExemptionUserSetting";
      break;
    case ExemptionReason::k3PCDMetadata:
      reason = "Exemption3PCDMetadata";
      break;
    case ExemptionReason::k3PCDDeprecationTrial:
      reason = "Exemption3PCDDeprecationTrial";
      break;
    case ExemptionReason::kTopLevel3PCDDeprecationTrial:
      reason = "ExemptionTopLevel3PCDDeprecationTrial";
      break;
    case ExemptionReason::k3PCDHeuristics:
      reason = "Exemption3PCDHeuristics";
      break;
    case ExemptionReason::kEnterprisePolicy:
      reason = "ExemptionEnterprisePolicy";
      break;
    case ExemptionReason::kStorageAccess:
      reason = "ExemptionStorageAccess";
      break;
    case ExemptionReason::kTopLevelStorageAccess:
      reason = "ExemptionTopLevelStorageAccess";
      break;
    case ExemptionReason::kScheme:
      reason = "ExemptionScheme";
      break;
    case ExemptionReason::kSameSiteNoneCookiesInSandbox:
      reason = "ExemptionSameSiteNoneCookiesInSandbox";
      break;
  }
  base::StrAppend(&out, {reason});

  return out;
}

bool CookieInclusionStatus::HasExactlyExclusionReasonsForTesting(
    ExclusionReasonBitset reasons) const {
  CookieInclusionStatus expected = MakeFromReasonsForTesting(reasons);
  return expected.exclusion_reasons_ == exclusion_reasons_;
}

bool CookieInclusionStatus::HasExactlyWarningReasonsForTesting(
    WarningReasonBitset reasons) const {
  CookieInclusionStatus expected = MakeFromReasonsForTesting({}, reasons);
  return expected.warning_reasons_ == warning_reasons_;
}

CookieInclusionStatus CookieInclusionStatus::MakeFromReasonsForTesting(
    ExclusionReasonBitset exclusions,
    WarningReasonBitset warnings,
    ExemptionReason exemption) {
  CookieInclusionStatus status;
  for (ExclusionReason reason : exclusions) {
    status.AddExclusionReason(reason);
  }
  for (WarningReason warning : warnings) {
    status.AddWarningReason(warning);
  }
  status.MaybeSetExemptionReason(exemption);

  for (auto reason : exclusions) {
    CHECK(status.HasExclusionReason(reason))
        << "Exemption " << static_cast<int>(reason) << " could not be applied";
  }
  CHECK_EQ(status.exclusion_reasons_.size(), exclusions.size());
  for (auto reason : warnings) {
    CHECK(status.HasWarningReason(reason))
        << "Warning " << static_cast<int>(reason) << " could not be applied";
  }
  CHECK_EQ(status.warning_reasons_.size(), warnings.size());
  CHECK_EQ(status.exemption_reason(), exemption)
      << "Exemption " << static_cast<int>(exemption) << " could not be applied";

  return status;
}

std::optional<CookieInclusionStatus> CookieInclusionStatus::MakeFromComponents(
    ExclusionReasonBitset exclusions,
    WarningReasonBitset warnings,
    ExemptionReason exemption) {
  CookieInclusionStatus status;
  for (ExclusionReason reason : exclusions) {
    status.AddExclusionReason(reason);
  }
  for (WarningReason warning : warnings) {
    status.AddWarningReason(warning);
  }
  status.MaybeSetExemptionReason(exemption);

  if (status.exclusion_reasons() != exclusions ||
      status.warning_reasons() != warnings ||
      status.exemption_reason() != exemption) {
    return std::nullopt;
  }
  return status;
}

bool CookieInclusionStatus::ExcludedByUserPreferencesOrTPCD() const {
  if (HasOnlyExclusionReason(ExclusionReason::EXCLUDE_USER_PREFERENCES) ||
      HasOnlyExclusionReason(ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
    return true;
  }
  return exclusion_reasons_ ==
         ExclusionReasonBitset(
             {ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT,
              ExclusionReason::
                  EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET});
}

}  // namespace net
