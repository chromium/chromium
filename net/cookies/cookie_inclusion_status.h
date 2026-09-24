// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_INCLUSION_STATUS_H_
#define NET_COOKIES_COOKIE_INCLUSION_STATUS_H_

#include <stdint.h>

#include <bitset>
#include <cstdint>
#include <ostream>
#include <string>

#include "base/containers/enum_set.h"
#include "net/base/net_export.h"

namespace net {

// This class represents if a cookie was included or excluded in a cookie get or
// set operation, and if excluded why. It holds a set of reasons for
// exclusion, where cookie inclusion is represented by the absence of any
// exclusion reasons. Also marks whether a cookie should be warned about, e.g.
// for deprecation or intervention reasons.
// TODO(crbug.com/40219875): Improve serialization validation comments.
class NET_EXPORT CookieInclusionStatus {
 public:
  // Types of reasons why a cookie might be excluded.
  enum class ExclusionReason {
    EXCLUDE_UNKNOWN_ERROR = 0,

    // Statuses applied when accessing a cookie (either sending or setting):

    // Cookie was HttpOnly, but the attempted access was through a non-HTTP API.
    EXCLUDE_HTTP_ONLY,
    // Cookie was Secure, but the URL was not allowed to access Secure cookies.
    EXCLUDE_SECURE_ONLY,
    // The cookie's domain attribute did not match the domain of the URL
    // attempting access.
    EXCLUDE_DOMAIN_MISMATCH,
    // The cookie's path attribute did not match the path of the URL attempting
    // access.
    EXCLUDE_NOT_ON_PATH,
    // The cookie had SameSite=Strict, and the attempted access did not have an
    // appropriate SameSiteCookieContext.
    EXCLUDE_SAMESITE_STRICT,
    // The cookie had SameSite=Lax, and the attempted access did not have an
    // appropriate SameSiteCookieContext.
    EXCLUDE_SAMESITE_LAX,
    // The cookie did not specify a SameSite attribute, and therefore was
    // treated as if it were SameSite=Lax, and the attempted access did not have
    // an appropriate SameSiteCookieContext.
    EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX,
    // The cookie specified SameSite=None, but it was not Secure.
    EXCLUDE_SAMESITE_NONE_INSECURE,
    // Caller did not allow access to the cookie.
    EXCLUDE_USER_PREFERENCES,

    // Statuses only applied when creating/setting cookies:

    // Cookie was malformed and could not be stored, due to problem(s) while
    // parsing.
    // TODO(crbug.com/40189703): Use more specific reasons for parsing errors.
    EXCLUDE_FAILURE_TO_STORE,
    // Attempted to set a cookie from a scheme that does not support cookies.
    EXCLUDE_NONCOOKIEABLE_SCHEME,
    // Cookie would have overwritten a Secure cookie, and was not allowed to do
    // so. (See "Leave Secure Cookies Alone":
    // https://tools.ietf.org/html/draft-west-leave-secure-cookies-alone-05 )
    EXCLUDE_OVERWRITE_SECURE,
    // Cookie would have overwritten an HttpOnly cookie, and was not allowed to
    // do so.
    EXCLUDE_OVERWRITE_HTTP_ONLY,
    // Cookie was set with an invalid Domain attribute.
    EXCLUDE_INVALID_DOMAIN,
    // Cookie was set with an invalid __Host- or __Secure- prefix.
    EXCLUDE_INVALID_PREFIX,
    /// Cookie was set with an invalid Partitioned attribute, which is only
    // valid if the cookie has a __Host- prefix.
    EXCLUDE_INVALID_PARTITIONED,
    // Cookie exceeded the name/value pair size limit.
    EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE,
    // Cookie exceeded the attribute size limit. Note that this exclusion value
    // won't be used by code that parses cookie lines since RFC6265bis
    // indicates that large attributes should be ignored instead of causing the
    // whole cookie to be rejected. There will be a corresponding WarningReason
    // to notify users that an attribute value was ignored in that case.
    EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE,
    // Cookie was set with a Domain attribute containing non ASCII characters.
    EXCLUDE_DOMAIN_NON_ASCII,
    // Cookie's source_port did not match the port of the request.
    EXCLUDE_PORT_MISMATCH,
    // Cookie's source_scheme did not match the scheme of the request.
    EXCLUDE_SCHEME_MISMATCH,
    // Cookie is a domain cookie and has the same name as an origin cookie on
    // this origin.
    EXCLUDE_SHADOWING_DOMAIN,
    // Cookie contains ASCII control characters (including the tab character,
    // when it appears in the middle of the cookie name, value, an attribute
    // name, or an attribute value).
    EXCLUDE_DISALLOWED_CHARACTER,
    // Cookie is blocked for third-party cookie phaseout.
    EXCLUDE_THIRD_PARTY_PHASEOUT,
    // Cookie contains no content or only whitespace.
    EXCLUDE_NO_COOKIE_CONTENT,
    // Cookie is unpartitioned and being accessed from an anonymous context
    EXCLUDE_ANONYMOUS_CONTEXT,
    // Cookie was set with an invalid Path attribute (path was modified during
    // canonicalization, indicating the original path was malformed).
    EXCLUDE_INVALID_PATH,
    // Cookie was rejected in parsing due to having an ambiguous serialization.
    // This can result from having an empty name and a value containing an
    // equals sign, such as a cookie line "=Foo=Bar", which is serialized as
    // "Foo=Bar" and could shadow a cookie named "Foo".
    EXCLUDE_AMBIGUOUS_SERIALIZATION,
    // This should be kept last.
    MAX_EXCLUSION_REASON = EXCLUDE_AMBIGUOUS_SERIALIZATION
  };

  // Reason to warn about a cookie. Any information contained in
  // WarningReason of an included cookie may be passed to an untrusted
  // renderer.
  enum class WarningReason {
    // Of the following 3 SameSite warnings, there will be, at most, a single
    // active one.

    // Warn if a cookie with unspecified SameSite attribute is used in a
    // cross-site context.
    WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT = 0,
    // Warn if a cookie with SameSite=None is not Secure.
    WARN_SAMESITE_NONE_INSECURE,
    // Warn if a cookie with unspecified SameSite attribute is defaulted into
    // Lax and is sent on a request with unsafe method, only because it is new
    // enough to activate the Lax-allow-unsafe intervention.
    WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE,

    // Advisory warning attached when a Secure cookie is accessed from (sent to,
    // or set by) a non-cryptographic URL. This can happen if the URL is
    // potentially trustworthy (e.g. a localhost URL, or another URL that
    // the CookieAccessDelegate is configured to allow). This also applies to
    // cookies with secure source schemes when scheme binding is enabled.
    // TODO(chlily): Add metrics for how often and where this occurs.
    WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC,

    // The cookie would have been included prior to the spec change considering
    // redirects in the SameSite context calculation
    // (https://github.com/httpwg/http-extensions/pull/1348)
    // but would have been excluded after the spec change, due to a cross-site
    // redirect causing the SameSite context calculation to be downgraded.
    // This is applied if and only if the cookie's inclusion was changed by
    // considering redirect chains (and is applied regardless of which context
    // was actually used for the inclusion decision). This is not applied if
    // the context was downgraded but the cookie would have been
    // included/excluded in both cases.
    WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION,

    // Separately record when a cookie would have been included prior to the
    // spec change considering redirects in the SameSite context computation
    // (see above reason) when the request or response had no initiator.
    WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION_NO_INITIATOR,

    // The cookie exceeded the attribute size limit. RFC6265bis indicates
    // that large attributes should be ignored instead of causing the whole
    // cookie to be rejected. This is applied by the code that parses cookie
    // lines and notifies the user that an attribute value was ignored.
    WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE,

    // The cookie was set with a Domain attribute containing non ASCII
    // characters.
    WARN_DOMAIN_NON_ASCII,
    // The cookie's source_port did not match the port of the request.
    WARN_PORT_MISMATCH,
    // The cookie's source_scheme did not match the scheme of the request.
    WARN_SCHEME_MISMATCH,
    // The cookie's creation url is non-cryptographic but it specified the
    // "Secure" attribute. A trustworthy url may be setting this cookie, but we
    // can't confirm/deny that at the time of creation.
    WARN_TENTATIVELY_ALLOWING_SECURE_SOURCE_SCHEME,
    // Cookie is a domain cookie and has the same name as an origin cookie on
    // this origin. This cookie would be blocked if shadowing protection was
    // enabled.
    WARN_SHADOWING_DOMAIN,

    // This cookie will be blocked for third-party cookie phaseout.
    WARN_THIRD_PARTY_PHASEOUT,

    // This should be kept last.
    MAX_WARNING_REASON = WARN_THIRD_PARTY_PHASEOUT
  };

  // Types of reasons why a cookie should-have-been-blocked by 3pcd got
  // exempted and included.
  enum class ExemptionReason {
    // The default exemption reason. The cookie with this reason could either be
    // included, or blocked due to 3pcd-unrelated reasons.
    kNone = 0,
    // For user explicit settings, including User bypass.
    kUserSetting,
    // For Enterprise Policy : CookieAllowedForUrls and BlockThirdPartyCookies.
    kEnterprisePolicy,
    kStorageAccess,
    kTopLevelStorageAccess,
    // Allowed by the scheme.
    kScheme,
    // Allowed by the sandbox 'allow-same-site-none-cookies' value.
    kSameSiteNoneCookiesInSandbox,
  };

  using ExclusionReasonBitset =
      base::EnumSet<ExclusionReason,
                    ExclusionReason::EXCLUDE_UNKNOWN_ERROR,
                    ExclusionReason::MAX_EXCLUSION_REASON>;
  // Mojom and some tests assume that all the exclusion reasons will fit within
  // a uint64_t. Once that's not longer true those assumptions need to be
  // updated (along with this assert).
  static_assert(ExclusionReasonBitset::kValueCount <= 64,
                "Expanding ExclusionReasons past 64 reasons requires updating "
                "usage assumptions.");
  using WarningReasonBitset =
      base::EnumSet<WarningReason,
                    WarningReason::WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
                    WarningReason::MAX_WARNING_REASON>;
  // Mojom and some tests assume that all the warning reasons will fit within
  // a uint64_t. Once that's not longer true those assumptions need to be
  // updated (along with this assert).
  static_assert(WarningReasonBitset::kValueCount <= 64,
                "Expanding WarningReasons past 64 reasons requires updating "
                "usage assumptions.");

  // Makes a status that says include and should not warn.
  CookieInclusionStatus();

  // Copyable.
  CookieInclusionStatus(const CookieInclusionStatus& other);
  CookieInclusionStatus& operator=(const CookieInclusionStatus& other);

  bool operator==(const CookieInclusionStatus& other) const;

  // Whether the status is to include the cookie, and has no other reasons for
  // exclusion.
  bool IsInclude() const;

  // Whether the given reason for exclusion is present.
  bool HasExclusionReason(ExclusionReason status_type) const;

  // Whether the given reason for exclusion is present, and is the ONLY reason
  // for exclusion.
  bool HasOnlyExclusionReason(ExclusionReason status_type) const;

  // Add an exclusion reason. CHECKs if `status_type` is out of range.
  void AddExclusionReason(ExclusionReason status_type);

  // Remove an exclusion reason. CHECKs if `reason` is out of range.
  void RemoveExclusionReason(ExclusionReason reason);

  // Remove multiple exclusion reasons.
  void RemoveExclusionReasons(ExclusionReasonBitset reasons);

  // Only updates exemption reason if the cookie was not already excluded and
  // doesn't already have an exemption reason.
  void MaybeSetExemptionReason(ExemptionReason reason);

  ExemptionReason exemption_reason() const { return exemption_reason_; }

  // If the cookie would have been excluded for reasons other than
  // SameSite-related reasons, don't bother warning about it (clear the
  // warning).
  void MaybeClearSameSiteWarning();

  // Whether to record the breaking downgrade metrics if the cookie is included
  // or if it's only excluded because of insufficient same-site context.
  bool ShouldRecordDowngradeMetrics() const;

  // Whether the cookie should be warned about.
  bool ShouldWarn() const;

  // Whether the given reason for warning is present.
  bool HasWarningReason(WarningReason reason) const;

  // Add an warning reason. CHECKs if `reason` is out of range.
  void AddWarningReason(WarningReason reason);

  // Remove an warning reason. CHECKs if `reason` is out of range.
  void RemoveWarningReason(WarningReason reason);

  // Used for serialization/deserialization.
  ExclusionReasonBitset exclusion_reasons() const { return exclusion_reasons_; }

  WarningReasonBitset warning_reasons() const { return warning_reasons_; }

  // Get exclusion reason(s) and warning in string format.
  std::string GetDebugString() const;

  // Checks whether the exclusion reasons are exactly the set of exclusion
  // reasons in the set. (Ignores warnings.)
  bool HasExactlyExclusionReasonsForTesting(
      ExclusionReasonBitset reasons) const;

  // Checks whether the warning reasons are exactly the set of warning
  // reasons in the set. (Ignores exclusions.)
  bool HasExactlyWarningReasonsForTesting(WarningReasonBitset reasons) const;

  // Makes a status that contains the given reasons. If the given reasons are
  // self-inconsistent, CHECKs.
  static CookieInclusionStatus MakeFromReasonsForTesting(
      ExclusionReasonBitset exclusions,
      WarningReasonBitset warnings = WarningReasonBitset(),
      ExemptionReason exemption = ExemptionReason::kNone);

  static std::optional<CookieInclusionStatus> MakeFromComponents(
      ExclusionReasonBitset exclusions,
      WarningReasonBitset warnings,
      ExemptionReason exemption);

  // Returns true if the cookie was excluded because of user preferences or
  // 3PCD.
  bool ExcludedByUserPreferencesOrTPCD() const;

  void ResetForTesting() {
    exclusion_reasons_.Clear();
    warning_reasons_.Clear();
    exemption_reason_ = ExemptionReason::kNone;
  }

 private:
  // Returns the `exclusion_reasons_` with the given `reasons` unset.
  ExclusionReasonBitset ExclusionReasonsWithout(
      ExclusionReasonBitset reasons) const;

  // If the cookie would have been excluded by reasons that are not
  // Third-party cookie phaseout related, clear the Third-party cookie phaseout
  // warning/exclusion reason in this case.
  void MaybeClearThirdPartyPhaseoutReason();

  // A bitset of the applicable exclusion reasons.
  ExclusionReasonBitset exclusion_reasons_;

  // A bitset of the applicable warning reasons.
  WarningReasonBitset warning_reasons_;

  // A cookie can only have at most one exemption reason.
  ExemptionReason exemption_reason_ = ExemptionReason::kNone;
};

NET_EXPORT inline std::ostream& operator<<(
    std::ostream& os,
    const CookieInclusionStatus& status) {
  return os << status.GetDebugString();
}

// Provided to allow gtest to create more helpful error messages, instead of
// printing hex.
inline void PrintTo(const CookieInclusionStatus& cis, std::ostream* os) {
  *os << cis;
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_INCLUSION_STATUS_H_
