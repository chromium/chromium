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
    RemoveWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT);
    RemoveWarningReason(CookieInclusionStatus::WARN_SAMESITE_NONE_INSECURE);
    RemoveWarningReason(
        CookieInclusionStatus::WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE);
  }

  if (!ShouldRecordDowngradeMetrics()) {
    RemoveWarningReason(
        CookieInclusionStatus::WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(
        CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(
        CookieInclusionStatus::WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE);
    RemoveWarningReason(
        CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE);
    RemoveWarningReason(
        CookieInclusionStatus::WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE);
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

  // Inclusion/exclusion
  if (IsInclude())
    base::StrAppend(&out, {"INCLUDE, "});
  if (HasExclusionReason(EXCLUDE_UNKNOWN_ERROR))
    base::StrAppend(&out, {"EXCLUDE_UNKNOWN_ERROR, "});
  if (HasExclusionReason(EXCLUDE_HTTP_ONLY))
    base::StrAppend(&out, {"EXCLUDE_HTTP_ONLY, "});
  if (HasExclusionReason(EXCLUDE_SECURE_ONLY))
    base::StrAppend(&out, {"EXCLUDE_SECURE_ONLY, "});
  if (HasExclusionReason(EXCLUDE_DOMAIN_MISMATCH))
    base::StrAppend(&out, {"EXCLUDE_DOMAIN_MISMATCH, "});
  if (HasExclusionReason(EXCLUDE_NOT_ON_PATH))
    base::StrAppend(&out, {"EXCLUDE_NOT_ON_PATH, "});
  if (HasExclusionReason(EXCLUDE_SAMESITE_STRICT))
    base::StrAppend(&out, {"EXCLUDE_SAMESITE_STRICT, "});
  if (HasExclusionReason(EXCLUDE_SAMESITE_LAX))
    base::StrAppend(&out, {"EXCLUDE_SAMESITE_LAX, "});
  if (HasExclusionReason(EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX))
    base::StrAppend(&out, {"EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX, "});
  if (HasExclusionReason(EXCLUDE_SAMESITE_NONE_INSECURE))
    base::StrAppend(&out, {"EXCLUDE_SAMESITE_NONE_INSECURE, "});
  if (HasExclusionReason(EXCLUDE_USER_PREFERENCES))
    base::StrAppend(&out, {"EXCLUDE_USER_PREFERENCES, "});
  if (HasExclusionReason(EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT))
    base::StrAppend(&out, {"EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT, "});
  if (HasExclusionReason(EXCLUDE_FAILURE_TO_STORE))
    base::StrAppend(&out, {"EXCLUDE_FAILURE_TO_STORE, "});
  if (HasExclusionReason(EXCLUDE_NONCOOKIEABLE_SCHEME))
    base::StrAppend(&out, {"EXCLUDE_NONCOOKIEABLE_SCHEME, "});
  if (HasExclusionReason(EXCLUDE_OVERWRITE_SECURE))
    base::StrAppend(&out, {"EXCLUDE_OVERWRITE_SECURE, "});
  if (HasExclusionReason(EXCLUDE_OVERWRITE_HTTP_ONLY))
    base::StrAppend(&out, {"EXCLUDE_OVERWRITE_HTTP_ONLY, "});
  if (HasExclusionReason(EXCLUDE_INVALID_DOMAIN))
    base::StrAppend(&out, {"EXCLUDE_INVALID_DOMAIN, "});
  if (HasExclusionReason(EXCLUDE_INVALID_PREFIX))
    base::StrAppend(&out, {"EXCLUDE_INVALID_PREFIX, "});
  if (HasExclusionReason(EXCLUDE_INVALID_SAMEPARTY))
    base::StrAppend(&out, {"EXCLUDE_INVALID_SAMEPARTY, "});

  // Add warning
  if (!ShouldWarn()) {
    base::StrAppend(&out, {"DO_NOT_WARN"});
    return out;
  }

  if (HasWarningReason(WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT))
    base::StrAppend(&out, {"WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT, "});
  if (HasWarningReason(WARN_SAMESITE_NONE_INSECURE))
    base::StrAppend(&out, {"WARN_SAMESITE_NONE_INSECURE, "});
  if (HasWarningReason(WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE))
    base::StrAppend(&out, {"WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE, "});
  if (HasWarningReason(WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE))
    base::StrAppend(&out, {"WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE, "});
  if (HasWarningReason(WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE))
    base::StrAppend(&out, {"WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE, "});
  if (HasWarningReason(WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE))
    base::StrAppend(&out, {"WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE, "});
  if (HasWarningReason(WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE))
    base::StrAppend(&out, {"WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE, "});
  if (HasWarningReason(WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE))
    base::StrAppend(&out, {"WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE, "});
  if (HasWarningReason(WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC))
    base::StrAppend(&out, {"WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC, "});
  if (HasWarningReason(WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE))
    base::StrAppend(&out, {"WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE, "});
  if (HasWarningReason(WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE))
    base::StrAppend(&out, {"WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE, "});
  if (HasWarningReason(WARN_SAMESITE_LAX_EXCLUDED_AFTER_BUGFIX_1166211)) {
    base::StrAppend(&out,
                    {"WARN_SAMESITE_LAX_EXCLUDED_AFTER_BUGFIX_1166211, "});
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
