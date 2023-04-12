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
#include <vector>

#include "net/base/net_export.h"

class GURL;

namespace net {

// This class represents if a cookie was included or excluded in a cookie get or
// set operation, and if excluded why. It holds a vector of reasons for
// exclusion, where cookie inclusion is represented by the absence of any
// exclusion reasons. Also marks whether a cookie should be warned about, e.g.
// for deprecation or intervention reasons.
// TODO(crbug.com/1310444): Improve serialization validation comments.
class NET_EXPORT CookieInclusionStatus {
 public:
  // Types of reasons why a cookie might be excluded.
  // If adding a ExclusionReason, please also update the GetDebugString()
  // method.
  enum ExclusionReason {
    EXCLUDE_UNKNOWN_ERROR = 0,

    // Statuses applied when accessing a cookie (either sending or setting):

    // Cookie was HttpOnly, but the attempted access was through a non-HTTP API.
    EXCLUDE_HTTP_ONLY = 1,
    // Cookie was Secure, but the URL was not allowed to access Secure cookies.
    EXCLUDE_SECURE_ONLY = 2,
    // The cookie's domain attribute did not match the domain of the URL
    // attempting access.
    EXCLUDE_DOMAIN_MISMATCH = 3,
    // The cookie's path attribute did not match the path of the URL attempting
    // access.
    EXCLUDE_NOT_ON_PATH = 4,
    // The cookie had SameSite=Strict, and the attempted access did not have an
    // appropriate SameSiteCookieContext.
    EXCLUDE_SAMESITE_STRICT = 5,
    // The cookie had SameSite=Lax, and the attempted access did not have an
    // appropriate SameSiteCookieContext.
    EXCLUDE_SAMESITE_LAX = 6,
    // The cookie did not specify a SameSite attribute, and therefore was
    // treated as if it were SameSite=Lax, and the attempted access did not have
    // an appropriate SameSiteCookieContext.
    EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX = 7,
    // The cookie specified SameSite=None, but it was not Secure.
    EXCLUDE_SAMESITE_NONE_INSECURE = 8,
    // Caller did not allow access to the cookie.
    EXCLUDE_USER_PREFERENCES = 9,
    // The cookie specified SameParty, but was used in a cross-party context.
    EXCLUDE_SAMEPARTY_CROSS_PARTY_CONTEXT = 10,

    // Statuses only applied when creating/setting cookies:

    // Cookie was malformed and could not be stored, due to problem(s) while
    // parsing.
    // TODO(crbug.com/1228815): Use more specific reasons for parsing errors.
    EXCLUDE_FAILURE_TO_STORE = 11,
    // Attempted to set a cookie from a scheme that does not support cookies.
    EXCLUDE_NONCOOKIEABLE_SCHEME = 12,
    // Cookie would have overwritten a Secure cookie, and was not allowed to do
    // so. (See "Leave Secure Cookies Alone":
    // https://tools.ietf.org/html/draft-west-leave-secure-cookies-alone-05 )
    EXCLUDE_OVERWRITE_SECURE = 13,
    // Cookie would have overwritten an HttpOnly cookie, and was not allowed to
    // do so.
    EXCLUDE_OVERWRITE_HTTP_ONLY = 14,
    // Cookie was set with an invalid Domain attribute.
    EXCLUDE_INVALID_DOMAIN = 15,
    // Cookie was set with an invalid __Host- or __Secure- prefix.
    EXCLUDE_INVALID_PREFIX = 16,
    // Cookie was set with an invalid SameParty attribute in combination with
    // other attributes. (SameParty is invalid if Secure is not present, or if
    // SameSite=Strict is present.)
    EXCLUDE_INVALID_SAMEPARTY = 17,
    /// Cookie was set with an invalid Partitioned attribute, which is only
    // valid if the cookie has a __Host- prefix and does not have the SameParty
    // attribute.
    EXCLUDE_INVALID_PARTITIONED = 18,
    // Cookie exceeded the name/value pair size limit.
    EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE = 19,
    // Cookie exceeded the attribute size limit. Note that this exclusion value
    // won't be used by code that parses cookie lines since RFC6265bis
    // indicates that large attributes should be ignored instead of causing the
    // whole cookie to be rejected. There will be a corresponding WarningReason
    // to notify users that an attribute value was ignored in that case.
    EXCLUDE_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE = 20,
    // Cookie was set with a Domain attribute containing non ASCII characters.
    EXCLUDE_DOMAIN_NON_ASCII = 21,
    // Special case for when a cookie is blocked by third-party cookie blocking
    // but the two sites are in the same First-Party Set.
    EXCLUDE_THIRD_PARTY_BLOCKED_WITHIN_FIRST_PARTY_SET = 22,

    // This should be kept last.
    NUM_EXCLUSION_REASONS
  };

  // Reason to warn about a cookie. Any information contained in WarningReason
  // of an included cookie may be passed to an untrusted renderer.
  // If you add one, please update GetDebugString().
  enum WarningReason {
    // Of the following 3 SameSite warnings, there will be, at most, a single
    // active one.

    // Warn if a cookie with unspecified SameSite attribute is used in a
    // cross-site context.
    WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT = 0,
    // Warn if a cookie with SameSite=None is not Secure.
    WARN_SAMESITE_NONE_INSECURE = 1,
    // Warn if a cookie with unspecified SameSite attribute is defaulted into
    // Lax and is sent on a request with unsafe method, only because it is new
    // enough to activate the Lax-allow-unsafe intervention.
    WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE = 2,

    // The following warnings indicate that an included cookie with an effective
    // SameSite is experiencing a SameSiteCookieContext::|context| ->
    // SameSiteCookieContext::|schemeful_context| downgrade that will prevent
    // its access schemefully.
    // This situation means that a cookie is accessible when the
    // SchemefulSameSite feature is disabled but not when it's enabled,
    // indicating changed behavior and potential breakage.
    //
    // For example, a Strict to Lax downgrade for an effective SameSite=Strict
    // cookie:
    // This cookie would be accessible in the Strict context as its SameSite
    // value is Strict. However its context for schemeful same-site becomes Lax.
    // A strict cookie cannot be accessed in a Lax context and therefore the
    // behavior has changed.
    // As a counterexample, a Strict to Lax downgrade for an effective
    // SameSite=Lax cookie: A Lax cookie can be accessed in both Strict and Lax
    // contexts so there is no behavior change (and we don't warn about it).
    //
    // The warnings are in the following format:
    // WARN_{context}_{schemeful_context}_DOWNGRADE_{samesite_value}_SAMESITE
    //
    // Of the following 5 SameSite warnings, there will be, at most, a single
    // active one.

    // Strict to Lax downgrade for an effective SameSite=Strict cookie.
    // This warning is only applicable for cookies being sent because a Strict
    // cookie will be set in both Strict and Lax Contexts so the downgrade will
    // not affect it.
    WARN_STRICT_LAX_DOWNGRADE_STRICT_SAMESITE = 3,
    // Strict to Cross-site downgrade for an effective SameSite=Strict cookie.
    // This also applies to Strict to Lax Unsafe downgrades due to Lax Unsafe
    // behaving like Cross-site.
    WARN_STRICT_CROSS_DOWNGRADE_STRICT_SAMESITE = 4,
    // Strict to Cross-site downgrade for an effective SameSite=Lax cookie.
    // This also applies to Strict to Lax Unsafe downgrades due to Lax Unsafe
    // behaving like Cross-site.
    WARN_STRICT_CROSS_DOWNGRADE_LAX_SAMESITE = 5,
    // Lax to Cross-site downgrade for an effective SameSite=Strict cookie.
    // This warning is only applicable for cookies being set because a Strict
    // cookie will not be sent in a Lax context so the downgrade would not
    // affect it.
    WARN_LAX_CROSS_DOWNGRADE_STRICT_SAMESITE = 6,
    // Lax to Cross-site downgrade for an effective SameSite=Lax cookie.
    WARN_LAX_CROSS_DOWNGRADE_LAX_SAMESITE = 7,

    // Advisory warning attached when a Secure cookie is accessed from (sent to,
    // or set by) a non-cryptographic URL. This can happen if the URL is
    // potentially trustworthy (e.g. a localhost URL, or another URL that
    // the CookieAccessDelegate is configured to allow).
    // TODO(chlily): Add metrics for how often and where this occurs.
    WARN_SECURE_ACCESS_GRANTED_NON_CRYPTOGRAPHIC = 8,

    // The cookie was treated as SameParty. This is different from looking at
    // whether the cookie has the SameParty attribute, since we may choose to
    // ignore that attribute for one reason or another. E.g., we ignore the
    // SameParty attribute if the site is not a member of a nontrivial
    // First-Party Set.
    WARN_TREATED_AS_SAMEPARTY = 9,

    // The cookie was excluded solely for SameParty reasons (i.e. it was in
    // cross-party context), but would have been included by SameSite. (This can
    // only occur in cross-party, cross-site contexts, for cookies that are
    // 'SameParty; SameSite=None'.)
    WARN_SAMEPARTY_EXCLUSION_OVERRULED_SAMESITE = 10,

    // The cookie was included due to SameParty, even though it would have been
    // excluded by SameSite. (This can only occur in same-party, cross-site
    // contexts, for cookies that are 'SameParty; SameSite=Lax'.)
    WARN_SAMEPARTY_INCLUSION_OVERRULED_SAMESITE = 11,

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
    WARN_CROSS_SITE_REDIRECT_DOWNGRADE_CHANGES_INCLUSION = 12,

    // The cookie exceeded the attribute size limit. RFC6265bis indicates that
    // large attributes should be ignored instead of causing the whole cookie
    // to be rejected. This is applied by the code that parses cookie lines and
    // notifies the user that an attribute value was ignored.
    WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE = 13,

    // Cookie was set with a Domain attribute containing non ASCII characters.
    WARN_DOMAIN_NON_ASCII = 14,

    // This should be kept last.
    NUM_WARNING_REASONS
  };

  // These enums encode the context downgrade warnings + the secureness of the
  // url sending/setting the cookie. They're used for metrics only. The format
  // is k{context}{schemeful_context}{samesite_value}{securness}.
  // kNoDowngrade{securness} indicates that a cookie didn't have a breaking
  // context downgrade and was A) included B) excluded only due to insufficient
  // same-site context. I.e. the cookie wasn't excluded due to other reasons
  // such as third-party cookie blocking. Keep this in line with
  // SameSiteCookieContextBreakingDowngradeWithSecureness in enums.xml.
  enum class ContextDowngradeMetricValues {
    kNoDowngradeInsecure = 0,
    kNoDowngradeSecure = 1,

    kStrictLaxStrictInsecure = 2,
    kStrictCrossStrictInsecure = 3,
    kStrictCrossLaxInsecure = 4,
    kLaxCrossStrictInsecure = 5,
    kLaxCrossLaxInsecure = 6,

    kStrictLaxStrictSecure = 7,
    kStrictCrossStrictSecure = 8,
    kStrictCrossLaxSecure = 9,
    kLaxCrossStrictSecure = 10,
    kLaxCrossLaxSecure = 11,

    // Keep last.
    kMaxValue = kLaxCrossLaxSecure
  };

  using ExclusionReasonBitset =
      std::bitset<ExclusionReason::NUM_EXCLUSION_REASONS>;
  using WarningReasonBitset = std::bitset<WarningReason::NUM_WARNING_REASONS>;

  // Makes a status that says include and should not warn.
  CookieInclusionStatus();

  // Make a status that contains the given exclusion reason.
  explicit CookieInclusionStatus(ExclusionReason reason);
  // Makes a status that contains the given exclusion reason and warning.
  CookieInclusionStatus(ExclusionReason reason, WarningReason warning);
  // Makes a status that contains the given warning.
  explicit CookieInclusionStatus(WarningReason warning);

  // Copyable.
  CookieInclusionStatus(const CookieInclusionStatus& other);
  CookieInclusionStatus& operator=(const CookieInclusionStatus& other);

  bool operator==(const CookieInclusionStatus& other) const;
  bool operator!=(const CookieInclusionStatus& other) const;

  // Whether the status is to include the cookie, and has no other reasons for
  // exclusion.
  bool IsInclude() const;

  // Whether the given reason for exclusion is present.
  bool HasExclusionReason(ExclusionReason status_type) const;

  // Whether the given reason for exclusion is present, and is the ONLY reason
  // for exclusion.
  bool HasOnlyExclusionReason(ExclusionReason status_type) const;

  // Add an exclusion reason.
  void AddExclusionReason(ExclusionReason status_type);

  // Remove an exclusion reason.
  void RemoveExclusionReason(ExclusionReason reason);

  // Remove multiple exclusion reasons.
  void RemoveExclusionReasons(const std::vector<ExclusionReason>& reasons);

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

  // Whether a schemeful downgrade warning is present.
  // A schemeful downgrade means that an included cookie with an effective
  // SameSite is experiencing a SameSiteCookieContext::|context| ->
  // SameSiteCookieContext::|schemeful_context| downgrade that will prevent its
  // access schemefully. If the function returns true and |reason| is valid then
  // |reason| will contain which warning was found.
  bool HasDowngradeWarning(
      CookieInclusionStatus::WarningReason* reason = nullptr) const;

  // Add an warning reason.
  void AddWarningReason(WarningReason reason);

  // Remove an warning reason.
  void RemoveWarningReason(WarningReason reason);

  // Used for serialization/deserialization.
  ExclusionReasonBitset exclusion_reasons() const { return exclusion_reasons_; }
  void set_exclusion_reasons(ExclusionReasonBitset exclusion_reasons) {
    exclusion_reasons_ = exclusion_reasons;
  }

  WarningReasonBitset warning_reasons() const { return warning_reasons_; }
  void set_warning_reasons(WarningReasonBitset warning_reasons) {
    warning_reasons_ = warning_reasons;
  }

  ContextDowngradeMetricValues GetBreakingDowngradeMetricsEnumValue(
      const GURL& url) const;

  // Get exclusion reason(s) and warning in string format.
  std::string GetDebugString() const;

  // Checks whether the exclusion reasons are exactly the set of exclusion
  // reasons in the vector. (Ignores warnings.)
  bool HasExactlyExclusionReasonsForTesting(
      std::vector<ExclusionReason> reasons) const;

  // Checks whether the warning reasons are exactly the set of warning
  // reasons in the vector. (Ignores exclusions.)
  bool HasExactlyWarningReasonsForTesting(
      std::vector<WarningReason> reasons) const;

  // Validates mojo data, since mojo does not support bitsets.
  // TODO(crbug.com/1310444): Improve serialization validation comments
  // and check for mutually exclusive values.
  static bool ValidateExclusionAndWarningFromWire(uint32_t exclusion_reasons,
                                                  uint32_t warning_reasons);

  // Makes a status that contains the given exclusion reasons and warning.
  static CookieInclusionStatus MakeFromReasonsForTesting(
      std::vector<ExclusionReason> reasons,
      std::vector<WarningReason> warnings = std::vector<WarningReason>());

  // Returns true if the cookie was excluded because of user preferences.
  // HasOnlyExclusionReason(EXCLUDE_USER_PREFERENCES) will not return true for
  // third-party cookies blocked in sites in the same First-Party Set (note:
  // this is not the same as the cookie being blocked in a same-party context,
  // which takes the entire ancestor chain into account). See
  // https://crbug.com/1366868.
  bool ExcludedByUserPreferences() const;

 private:
  // Returns the `exclusion_reasons_` with the given `reasons` unset.
  ExclusionReasonBitset ExclusionReasonsWithout(
      const std::vector<ExclusionReason>& reasons) const;

  // A bit vector of the applicable exclusion reasons.
  ExclusionReasonBitset exclusion_reasons_;

  // A bit vector of the applicable warning reasons.
  WarningReasonBitset warning_reasons_;
};

NET_EXPORT inline std::ostream& operator<<(std::ostream& os,
                                           const CookieInclusionStatus status) {
  return os << status.GetDebugString();
}

// Provided to allow gtest to create more helpful error messages, instead of
// printing hex.
inline void PrintTo(const CookieInclusionStatus& cis, std::ostream* os) {
  *os << cis;
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_INCLUSION_STATUS_H_
