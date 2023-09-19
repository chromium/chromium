// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/cookie_inclusion_status.h"

#include <initializer_list>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "url/gurl.h"

namespace net {

CookieInclusionStatus::CookieInclusionStatus() = default;

CookieInclusionStatus::CookieInclusionStatus(ExclusionReason reason) {
  exclusion_reasons_[reason] = true;
}

CookieInclusionStatus::CookieInclusionStatus(ExclusionReason reason,
                                             WarningReason warning) {
  exclusion_reasons_[reason] = true;
  warning_reasons_[warning] = true;
}

CookieInclusionStatus::CookieInclusionStatus(WarningReason warning) {
  warning_reasons_[warning] = true;
}

CookieInclusionStatus::CookieInclusionStatus(
    const CookieInclusionStatus& other) = default;

CookieInclusionStatus& CookieInclusionStatus::operator=(
    const CookieInclusionStatus& other) = default;

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
  return exclusion_reasons_.none();
}

bool CookieInclusionStatus::HasExclusionReason(ExclusionReason reason) const {
  return exclusion_reasons_[reason];
}

bool CookieInclusionStatus::HasOnlyExclusionReason(
    ExclusionReason reason) const {
  return exclusion_reasons_[reason] && exclusion_reasons_.count() == 1;
}

void CookieInclusionStatus::AddExclusionReason(ExclusionReason reason) {
  exclusion_reasons_[reason] = true;
  // If the cookie would be excluded for reasons other than the new SameSite
  // rules, don't bother warning about it.
  MaybeClearSameSiteWarning();
}

void CookieInclusionStatus::RemoveExclusionReason(ExclusionReason reason) {
  exclusion_reasons_[reason] = false;
}

void CookieInclusionStatus::RemoveExclusionReasons(
    const std::vector<ExclusionReason>& reasons) {
  exclusion_reasons_ = ExclusionReasonsWithout(reasons);
}

CookieInclusionStatus::ExclusionReasonBitset
CookieInclusionStatus::ExclusionReasonsWithout(
    const std::vector<ExclusionReason>& reasons) const {
  CookieInclusionStatus::ExclusionReasonBitset result(exclusion_reasons_);
  for (const ExclusionReason reason : reasons) {
    result[reason] = false;
  }
  return result;
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
  return warning_reasons_.any();
}

bool CookieInclusionStatus::HasWarningReason(WarningReason reason) const {
  return warning_reasons_[reason];
}

bool CookieInclusionStatus::HasSchemefulDowngradeWarning(
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
  warning_reasons_[reason] = true;
}

void CookieInclusionStatus::RemoveWarningReason(WarningReason reason) {
  warning_reasons_[reason] = false;
}

CookieInclusionStatus::ContextDowngradeMetricValues
CookieInclusionStatus::GetBreakingDowngradeMetricsEnumValue(
    const GURL& url) const {
  bool url_is_secure = url.SchemeIsCryptographic();

  // Start the |reason| as something other than the downgrade warnings.
  WarningReason reason = WarningReason::NUM_WARNING_REASONS;

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
      {EXCLUDE_FAILURE_TO_STORE, "EXCLUDE_FAILURE_TO_STORE"},
      {EXCLUDE_NONCOOKIEABLE_SCHEME, "EXCLUDE_NONCOOKIEABLE_SCHEME"},
      {EXCLUDE_OVERWRITE_SECURE, "EXCLUDE_OVERWRITE_SECURE"},
      {EXCLUDE_OVERWRITE_HTTP_ONLY, "EXCLUDE_OVERWRITE_HTTP_ONLY"},
      {EXCLUDE_INVALID_DOMAIN, "EXCLUDE_INVALID_DOMAIN"},
      {EXCLUDE_INVALID_PREFIX, "EXCLUDE_INVALID_PREFIX"},
      {EXCLUDE_INVALID_PARTITIONED, "EXCLUDE_INVALID_PARTITIONED"},
      {EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE,
       "EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE"},
      {EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE,
       "EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE"},
      {EXCLUDE_DOMAIN_NON_ASCII, "EXCLUDE_DOMAIN_NON_ASCII"},
      {EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET,
       "EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET"},
      {EXCLUDE_PORT_MISMATCH, "EXCLUDE_PORT_MISMATCH"},
      {EXCLUDE_SCHEME_MISMATCH, "EXCLUDE_SCHEME_MISMATCH"},
      {EXCLUDE_SHADOWING_DOMAIN, "EXCLUDE_SHADOWING_DOMAIN"},
      {EXCLUDE_DISALLOWED_CHARACTER, "EXCLUDE_DISALLOWED_CHARACTER"},
      {EXCLUDE_THIRD_PARTY_PHASEOUT, "EXCLUDE_THIRD_PARTY_PHASEOUT"},
      {EXCLUDE_NO_COOKIE_CONTENT, "EXCLUDE_NO_COOKIE_CONTENT"},
  };
  static_assert(
      std::size(exclusion_reasons) == ExclusionReason::NUM_EXCLUSION_REASONS,
      "Please ensure all ExclusionReason variants are enumerated in "
      "GetDebugString");
  static_assert(base::ranges::is_sorted(exclusion_reasons),
                "Please keep the ExclusionReason variants sorted in numerical "
                "order in GetDebugString");

  for (const auto& reason : exclusion_reasons) {
    if (HasExclusionReason(reason.first))
      base::StrAppend(&out, {reason.second, ", "});
  }

  // Add warning
  if (!ShouldWarn()) {
    base::StrAppend(&out, {"DO_NOT_WARN"});
    return out;
  }

  constexpr std::pair<WarningReason, const char*> warning_reasons[] = {
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
      {WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION,
       "WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION"},
      {WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE,
       "WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE"},
      {WARN_DOMAIN_NON_ASCII, "WARN_DOMAIN_NON_ASCII"},
      {WARN_PORT_MISMATCH, "WARN_PORT_MISMATCH"},
      {WARN_SCHEME_MISMATCH, "WARN_SCHEME_MISMATCH"},
      {WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME,
       "WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME"},
      {WARN_SHADOWING_DOMAIN, "WARN_SHADOWING_DOMAIN"},
      {WARN_THIRD_PARTY_PHASEOUT, "WARN_THIRD_PARTY_PHASEOUT"},
  };
  static_assert(
      std::size(warning_reasons) == WarningReason::NUM_WARNING_REASONS,
      "Please ensure all WarningReason variants are enumerated in "
      "GetDebugString");
  static_assert(base::ranges::is_sorted(warning_reasons),
                "Please keep the WarningReason variants sorted in numerical "
                "order in GetDebugString");

  for (const auto& reason : warning_reasons) {
    if (HasWarningReason(reason.first))
      base::StrAppend(&out, {reason.second, ", "});
  }

  // Strip trailing comma and space.
  out.erase(out.end() - 2, out.end());

  return out;
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
bool CookieInclusionStatus::ValidateExclusionAndWarningFromWire(
    uint32_t exclusion_reasons,
    uint32_t warning_reasons) {
  uint32_t exclusion_mask =
      static_cast<uint32_t>(~0ul << ExclusionReason::NUM_EXCLUSION_REASONS);
  uint32_t warning_mask =
      static_cast<uint32_t>(~0ul << WarningReason::NUM_WARNING_REASONS);
  return (exclusion_reasons & exclusion_mask) == 0 &&
         (warning_reasons & warning_mask) == 0;
}

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

bool CookieInclusionStatus::ExcludedByUserPreferences() const {
  if (HasOnlyExclusionReason(ExclusionReason::EXCLUDE_USER_PREFERENCES) ||
      HasOnlyExclusionReason(ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT)) {
    return true;
  }
  return exclusion_reasons_.count() == 2 &&
         (exclusion_reasons_[ExclusionReason::EXCLUDE_USER_PREFERENCES] ||
          exclusion_reasons_[ExclusionReason::EXCLUDE_THIRD_PARTY_PHASEOUT]) &&
         exclusion_reasons_
             [ExclusionReason::
                  EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET];
}

}  // namespace net
