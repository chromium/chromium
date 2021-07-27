// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_inclusion_status.h"

#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace net {

namespace {

uint32_t GetExclusionBitmask(CookieInclusionStatus::ExclusionReason reason) {
  return 1u << static_cast<uint32_t>(reason);
}

uint32_t GetWarningBitmask(CookieInclusionStatus::WarningReason reason) {
  return 1u << static_cast<uint32_t>(reason);
}

}  // namespace

CookieInclusionStatus::CookieInclusionStatus() = default;

CookieInclusionStatus::CookieInclusionStatus(ExclusionReason reason)
    : exclusion_reasons_(GetExclusionBitmask(reason)) {}

CookieInclusionStatus::CookieInclusionStatus(ExclusionReason reason,
                                             WarningReason warning)
    : exclusion_reasons_(GetExclusionBitmask(reason)),
      warning_reasons_(GetWarningBitmask(warning)) {}

CookieInclusionStatus::CookieInclusionStatus(WarningReason warning)
    : warning_reasons_(GetWarningBitmask(warning)) {}

bool CookieInclusionStatus::operator==(
    const CookieInclusionStatus& other) const {
  return exclusion_reasons_ == other.exclusion_reasons_ &&
         warning_reasons_ == other.warning_reasons_;
}

bool CookieInclusionStatus::operator!=(
    const CookieInclusionStatus& other) const {
  return !operator==(other);
}

bool CookieInclusionStatus::IsInclude() const {
  return exclusion_reasons_ == 0u;
}

bool CookieInclusionStatus::HasExclusionReason(ExclusionReason reason) const {
  return exclusion_reasons_ & GetExclusionBitmask(reason);
}

bool CookieInclusionStatus::HasOnlyExclusionReason(
    ExclusionReason reason) const {
  return exclusion_reasons_ == GetExclusionBitmask(reason);
}

void CookieInclusionStatus::AddExclusionReason(ExclusionReason reason) {
  exclusion_reasons_ |= GetExclusionBitmask(reason);
  // If the cookie would be excluded for reasons other than the new SameSite
  // rules, don't bother warning about it.
  MaybeClearSameSiteWarning();
}

void CookieInclusionStatus::RemoveExclusionReason(ExclusionReason reason) {
  exclusion_reasons_ &= ~(GetExclusionBitmask(reason));
}

void CookieInclusionStatus::RemoveExclusionReasons(
    const std::vector<ExclusionReason>& reasons) {
  exclusion_reasons_ = ExclusionReasonsWithout(reasons);
}

uint32_t CookieInclusionStatus::ExclusionReasonsWithout(
    const std::vector<ExclusionReason>& reasons) const {
  uint32_t mask = 0u;
  for (const ExclusionReason reason : reasons) {
    mask |= GetExclusionBitmask(reason);
  }
  return exclusion_reasons_ & ~mask;
}

void CookieInclusionStatus::MaybeClearSameSiteWarning() {
  if (ExclusionReasonsWithout({
          EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
          EXCLUDE_SAMESITE_NONE_INSECURE,
      }) != 0u) {
    RemoveWarningReason(WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
    RemoveWarningReason(WARN_SAMESITE_NONE_INSECURE);
    RemoveWarningReason(WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);
  }

  if (!ShouldRecordDowngradeMetrics()) {
    RemoveWarningReason(WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE);
    RemoveWarningReason(WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE);

    RemoveWarningReason(WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION);
  }
}

bool CookieInclusionStatus::ShouldRecordDowngradeMetrics() const {
  return ExclusionReasonsWithout({
             EXCLUDE_SAMESITE_STRICT,
             EXCLUDE_SAMESITE_LAX,
             EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
         }) == 0u;
}

bool CookieInclusionStatus::ShouldWarn() const {
  return warning_reasons_ != 0u;
}

bool CookieInclusionStatus::HasWarningReason(WarningReason reason) const {
  return warning_reasons_ & GetWarningBitmask(reason);
}

bool CookieInclusionStatus::HasDowngradeWarning(
    CookieInclusionStatus::WarningReason* reason) const {
  if (!ShouldWarn())
    return false;

  const CookieInclusionStatus::WarningReason kDowngradeWarnings[] = {
      WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE,
      WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE,
      WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE,
      WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE,
      WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE,
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
  warning_reasons_ |= GetWarningBitmask(reason);
}

void CookieInclusionStatus::RemoveWarningReason(WarningReason reason) {
  warning_reasons_ &= ~(GetWarningBitmask(reason));
}

CookieInclusionStatus::ContextDowngradeMetricValues
CookieInclusionStatus::GetBreakingDowngradeMetricsEnumValue(
    const GURL& url) const {
  bool url_is_secure = url.SchemeIsCryptographic();

  // Start the |reason| as something other than the downgrade warnings.
  WarningReason reason = WarningReason::NUM_WARNING_REASONS;

  // Don't bother checking the return value because the default switch case
  // will handle if no reason was found.
  HasDowngradeWarning(&reason);

  switch (reason) {
    case WarningReason::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::STRICT_LAX_STRICT_SECURE
                 : ContextDowngradeMetricValues::STRICT_LAX_STRICT_INSECURE;
    case WarningReason::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::STRICT_CROSS_STRICT_SECURE
                 : ContextDowngradeMetricValues::STRICT_CROSS_STRICT_INSECURE;
    case WarningReason::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::STRICT_CROSS_LAX_SECURE
                 : ContextDowngradeMetricValues::STRICT_CROSS_LAX_INSECURE;
    case WarningReason::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::LAX_CROSS_STRICT_SECURE
                 : ContextDowngradeMetricValues::LAX_CROSS_STRICT_INSECURE;
    case WarningReason::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE:
      return url_is_secure
                 ? ContextDowngradeMetricValues::LAX_CROSS_LAX_SECURE
                 : ContextDowngradeMetricValues::LAX_CROSS_LAX_INSECURE;
    default:
      return url_is_secure
                 ? ContextDowngradeMetricValues::NO_DOWNGRADE_SECURE
                 : ContextDowngradeMetricValues::NO_DOWNGRADE_INSECURE;
  }
}

std::string CookieInclusionStatus::GetDebugString() const {
  std::string out;

  if (IsInclude())
    base::StrAppend(&out, {"INCLUDE, "});
  for (const auto& reason :
       std::initializer_list<std::pair<ExclusionReason, std::string>>{
           {EXCLUDE_UNKNOWN_ERROR, "EXCLUDE_UNKNOWN_ERROR"},
           {EXCLUDE_HTTP_ONLY, "EXCLUDE_HTTP_ONLY"},
           {EXCLUDE_SECURE_ONLY, "EXCLUDE_SECURE_ONLY"},
           {EXCLUDE_DOMAIN_MISMATCH, "EXCLUDE_DOMAIN_MISMATCH"},
           {EXCLUDE_NOT_ON_PATH, "EXCLUDE_NOT_ON_PATH"},
           {EXCLUDE_SAMESITE_STRICT, "EXCLUDE_SAMESITE_STRICT"},
           {EXCLUDE_SAMESITE_LAX, "EXCLUDE_SAMESITE_LAX"},
           {EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
            "EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX"},
           {EXCLUDE_SAMESITE_NONE_INSECURE, "EXCLUDE_SAMESITE_NONE_INSECURE"},
           {EXCLUDE_USER_PREFERENCES, "EXCLUDE_USER_PREFERENCES"},
           {EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT,
            "EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT"},
           {EXCLUDE_FAILURE_TO_STORE, "EXCLUDE_FAILURE_TO_STORE"},
           {EXCLUDE_NONCOOKIEABLE_SCHEME, "EXCLUDE_NONCOOKIEABLE_SCHEME"},
           {EXCLUDE_OVERWRITE_SECURE, "EXCLUDE_OVERWRITE_SECURE"},
           {EXCLUDE_OVERWRITE_HTTP_ONLY, "EXCLUDE_OVERWRITE_HTTP_ONLY"},
           {EXCLUDE_INVALID_DOMAIN, "EXCLUDE_INVALID_DOMAIN"},
           {EXCLUDE_INVALID_PREFIX, "EXCLUDE_INVALID_PREFIX"},
           {EXCLUDE_INVALID_SAMEPARTY, "EXCLUDE_INVALID_SAMEPARTY"},
           {EXCLUDE_INVALID_PARTITIONED, "EXCLUDE_INVALID_PARTITIONED"},
       }) {
    if (HasExclusionReason(reason.first))
      base::StrAppend(&out, {reason.second, ", "});
  }

  // Add warning
  if (!ShouldWarn()) {
    base::StrAppend(&out, {"DO_NOT_WARN"});
    return out;
  }

  for (const auto& reason :
       std::initializer_list<std::pair<WarningReason, std::string>>{
           {WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
            "WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT"},
           {WARN_SAMESITE_NONE_INSECURE, "WARN_SAMESITE_NONE_INSECURE"},
           {WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE,
            "WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE"},
           {WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE,
            "WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE"},
           {WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE,
            "WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE"},
           {WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE,
            "WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE"},
           {WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE,
            "WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE"},
           {WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE,
            "WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE"},
           {WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC,
            "WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC"},
           {WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE,
            "WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE"},
           {WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE,
            "WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE"},
           {WARN_SAMESITE_NONE_REQUIRED, "WARN_SAMESITE_NONE_REQUIRED"},
           {WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_TOP_RESOURCE,
            "WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_TOP_RESOURCE"},
           {WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_ANCESTORS,
            "WARN_SAMESITE_NONE_INCLUDED_BY_SAMEPARTY_ANCESTORS"},
           {WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_LAX,
            "WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_LAX"},
           {WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_STRICT,
            "WARN_SAMESITE_NONE_INCLUDED_BY_SAMESITE_STRICT"},
           {WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION,
            "WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION"},
       }) {
    if (HasWarningReason(reason.first))
      base::StrAppend(&out, {reason.second, ", "});
  }

  // Strip trailing comma and space.
  out.erase(out.end() - 2, out.end());

  return out;
}

bool CookieInclusionStatus::IsValid() const {
  // Bit positions where there should not be any true bits.
  uint32_t exclusion_mask = ~0u << static_cast<int>(NUM_EXCLUSION_REASONS);
  uint32_t warning_mask = ~0u << static_cast<int>(NUM_WARNING_REASONS);
  return (exclusion_mask & exclusion_reasons_) == 0u &&
         (warning_mask & warning_reasons_) == 0u;
}

bool CookieInclusionStatus::HasExactlyExclusionReasonsForTesting(
    std::vector<CookieInclusionStatus::ExclusionReason> reasons) const {
  CookieInclusionStatus expected = MakeFromReasonsForTesting(reasons);
  return expected.exclusion_reasons_ == exclusion_reasons_;
}

bool CookieInclusionStatus::HasExactlyWarningReasonsForTesting(
    std::vector<WarningReason> reasons) const {
  CookieInclusionStatus expected = MakeFromReasonsForTesting({}, reasons);
  return expected.warning_reasons_ == warning_reasons_;
}

// static
CookieInclusionStatus CookieInclusionStatus::MakeFromReasonsForTesting(
    std::vector<ExclusionReason> reasons,
    std::vector<WarningReason> warnings) {
  CookieInclusionStatus status;
  for (ExclusionReason reason : reasons) {
    status.AddExclusionReason(reason);
  }
  for (WarningReason warning : warnings) {
    status.AddWarningReason(warning);
  }
  return status;
}

}  // namespace net
