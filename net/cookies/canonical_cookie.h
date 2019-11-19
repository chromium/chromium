// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_CANONICAL_COOKIE_H_
#define NET_COOKIES_CANONICAL_COOKIE_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"

class GURL;

namespace net {

class ParsedCookie;
class CanonicalCookie;

struct CookieWithStatus;
struct CookieAndLineWithStatus;

using CookieList = std::vector<CanonicalCookie>;
using CookieStatusList = std::vector<CookieWithStatus>;
using CookieAndLineStatusList = std::vector<CookieAndLineWithStatus>;

class NET_EXPORT CanonicalCookie {
 public:
  class CookieInclusionStatus;

  CanonicalCookie();
  CanonicalCookie(const CanonicalCookie& other);

  // This constructor does not validate or canonicalize their inputs;
  // the resulting CanonicalCookies should not be relied on to be canonical
  // unless the caller has done appropriate validation and canonicalization
  // themselves.
  // NOTE: Prefer using CreateSanitizedCookie() over directly using this
  // constructor.
  CanonicalCookie(const std::string& name,
                  const std::string& value,
                  const std::string& domain,
                  const std::string& path,
                  const base::Time& creation,
                  const base::Time& expiration,
                  const base::Time& last_access,
                  bool secure,
                  bool httponly,
                  CookieSameSite same_site,
                  CookiePriority priority);

  ~CanonicalCookie();

  // Supports the default copy constructor.

  // Creates a new |CanonicalCookie| from the |cookie_line| and the
  // |creation_time|.  Canonicalizes inputs.  May return nullptr if
  // an attribute value is invalid.  |url| must be valid.  |creation_time| may
  // not be null. Sets optional |status| to the relevant CookieInclusionStatus
  // if provided.  |server_time| indicates what the server sending us the Cookie
  // thought the current time was when the cookie was produced.  This is used to
  // adjust for clock skew between server and host.
  //
  // SameSite and HttpOnly related parameters are not checked here,
  // so creation of CanonicalCookies with e.g. SameSite=Strict from a cross-site
  // context is allowed. Create() also does not check whether |url| has a secure
  // scheme if attempting to create a Secure cookie. The Secure, SameSite, and
  // HttpOnly related parameters should be checked when setting the cookie in
  // the CookieStore.
  //
  // If a cookie is returned, |cookie->IsCanonical()| will be true.
  static std::unique_ptr<CanonicalCookie> Create(
      const GURL& url,
      const std::string& cookie_line,
      const base::Time& creation_time,
      base::Optional<base::Time> server_time,
      CookieInclusionStatus* status = nullptr);

  // Create a canonical cookie based on sanitizing the passed inputs in the
  // context of the passed URL.  Returns a null unique pointer if the inputs
  // cannot be sanitized.  If a cookie is created, |cookie->IsCanonical()|
  // will be true.
  static std::unique_ptr<CanonicalCookie> CreateSanitizedCookie(
      const GURL& url,
      const std::string& name,
      const std::string& value,
      const std::string& domain,
      const std::string& path,
      base::Time creation_time,
      base::Time expiration_time,
      base::Time last_access_time,
      bool secure,
      bool http_only,
      CookieSameSite same_site,
      CookiePriority priority);

  const std::string& Name() const { return name_; }
  const std::string& Value() const { return value_; }
  const std::string& Domain() const { return domain_; }
  const std::string& Path() const { return path_; }
  const base::Time& CreationDate() const { return creation_date_; }
  const base::Time& LastAccessDate() const { return last_access_date_; }
  bool IsPersistent() const { return !expiry_date_.is_null(); }
  const base::Time& ExpiryDate() const { return expiry_date_; }
  bool IsSecure() const { return secure_; }
  bool IsHttpOnly() const { return httponly_; }
  CookieSameSite SameSite() const { return same_site_; }
  CookiePriority Priority() const { return priority_; }
  bool IsDomainCookie() const {
    return !domain_.empty() && domain_[0] == '.'; }
  bool IsHostCookie() const { return !IsDomainCookie(); }

  bool IsExpired(const base::Time& current) const {
    return !expiry_date_.is_null() && current >= expiry_date_;
  }

  // Are the cookies considered equivalent in the eyes of RFC 2965.
  // The RFC says that name must match (case-sensitive), domain must
  // match (case insensitive), and path must match (case sensitive).
  // For the case insensitive domain compare, we rely on the domain
  // having been canonicalized (in
  // GetCookieDomainWithString->CanonicalizeHost).
  bool IsEquivalent(const CanonicalCookie& ecc) const {
    // It seems like it would make sense to take secure, httponly, and samesite
    // into account, but the RFC doesn't specify this.
    // NOTE: Keep this logic in-sync with TrimDuplicateCookiesForHost().
    return (name_ == ecc.Name() && domain_ == ecc.Domain()
            && path_ == ecc.Path());
  }

  // Returns a key such that two cookies with the same UniqueKey() are
  // guaranteed to be equivalent in the sense of IsEquivalent().
  std::tuple<std::string, std::string, std::string> UniqueKey() const {
    return std::make_tuple(name_, domain_, path_);
  }

  // Checks a looser set of equivalency rules than 'IsEquivalent()' in order
  // to support the stricter 'Secure' behaviors specified in
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone#section-3
  //
  // Returns 'true' if this cookie's name matches |ecc|, and this cookie is
  // a domain-match for |ecc| (or vice versa), and |ecc|'s path is "on" this
  // cookie's path (as per 'IsOnPath()').
  //
  // Note that while the domain-match cuts both ways (e.g. 'example.com'
  // matches 'www.example.com' in either direction), the path-match is
  // unidirectional (e.g. '/login/en' matches '/login' and '/', but
  // '/login' and '/' do not match '/login/en').
  bool IsEquivalentForSecureCookieMatching(const CanonicalCookie& ecc) const;

  void SetLastAccessDate(const base::Time& date) {
    last_access_date_ = date;
  }
  void SetCreationDate(const base::Time& date) { creation_date_ = date; }

  // Returns true if the given |url_path| path-matches the cookie-path as
  // described in section 5.1.4 in RFC 6265.
  bool IsOnPath(const std::string& url_path) const;

  // Returns true if the cookie domain matches the given |host| as described in
  // section 5.1.3 of RFC 6265.
  bool IsDomainMatch(const std::string& host) const;

  // Returns if the cookie should be included (and if not, why) for the given
  // request |url| using the CookieInclusionStatus enum. HTTP only cookies can
  // be filter by using appropriate cookie |options|. PLEASE NOTE that this
  // method does not check whether a cookie is expired or not!
  CookieInclusionStatus IncludeForRequestURL(
      const GURL& url,
      const CookieOptions& options,
      CookieAccessSemantics access_semantics =
          CookieAccessSemantics::UNKNOWN) const;

  // Returns if the cookie with given attributes can be set in context described
  // by |options|, and if no, describes why.
  // WARNING: this does not cover checking whether secure cookies are set in
  // a secure schema, since whether the schema is secure isn't part of
  // |options|.
  CookieInclusionStatus IsSetPermittedInContext(
      const CookieOptions& options,
      CookieAccessSemantics access_semantics =
          CookieAccessSemantics::UNKNOWN) const;

  std::string DebugString() const;

  static std::string CanonPathWithString(const GURL& url,
                                         const std::string& path_string);

  // Returns a "null" time if expiration was unspecified or invalid.
  static base::Time CanonExpiration(const ParsedCookie& pc,
                                    const base::Time& current,
                                    const base::Time& server_time);

  // Cookie ordering methods.

  // Returns true if the cookie is less than |other|, considering only name,
  // domain and path. In particular, two equivalent cookies (see IsEquivalent())
  // are identical for PartialCompare().
  bool PartialCompare(const CanonicalCookie& other) const;

  // Return whether this object is a valid CanonicalCookie().  Invalid
  // cookies may be constructed by the detailed constructor.
  // A cookie is considered canonical if-and-only-if:
  // * It can be created by CanonicalCookie::Create, or
  // * It is identical to a cookie created by CanonicalCookie::Create except
  //   that the creation time is null, or
  // * It can be derived from a cookie created by CanonicalCookie::Create by
  //   entry into and retrieval from a cookie store (specifically, this means
  //   by the setting of an creation time in place of a null creation time, and
  //   the setting of a last access time).
  // An additional requirement on a CanonicalCookie is that if the last
  // access time is non-null, the creation time must also be non-null and
  // greater than the last access time.
  bool IsCanonical() const;

  // Returns whether the effective SameSite mode is SameSite=None (i.e. no
  // SameSite restrictions).
  bool IsEffectivelySameSiteNone(CookieAccessSemantics access_semantics =
                                     CookieAccessSemantics::UNKNOWN) const;

  CookieEffectiveSameSite GetEffectiveSameSiteForTesting(
      CookieAccessSemantics access_semantics =
          CookieAccessSemantics::UNKNOWN) const;

  // Returns the cookie line (e.g. "cookie1=value1; cookie2=value2") represented
  // by |cookies|. The string is built in the same order as the given list.
  static std::string BuildCookieLine(const CookieList& cookies);

  // Same as above but takes a CookieStatusList (ignores the statuses).
  static std::string BuildCookieLine(const CookieStatusList& cookies);

 private:
  FRIEND_TEST_ALL_PREFIXES(CanonicalCookieTest, TestPrefixHistograms);

  // The special cookie prefixes as defined in
  // https://tools.ietf.org/html/draft-west-cookie-prefixes
  //
  // This enum is being histogrammed; do not reorder or remove values.
  enum CookiePrefix {
    COOKIE_PREFIX_NONE = 0,
    COOKIE_PREFIX_SECURE,
    COOKIE_PREFIX_HOST,
    COOKIE_PREFIX_LAST
  };

  // Returns the CookiePrefix (or COOKIE_PREFIX_NONE if none) that
  // applies to the given cookie |name|.
  static CookiePrefix GetCookiePrefix(const std::string& name);
  // Records histograms to measure how often cookie prefixes appear in
  // the wild and how often they would be blocked.
  static void RecordCookiePrefixMetrics(CookiePrefix prefix,
                                        bool is_cookie_valid);
  // Returns true if a prefixed cookie does not violate any of the rules
  // for that cookie.
  static bool IsCookiePrefixValid(CookiePrefix prefix,
                                  const GURL& url,
                                  const ParsedCookie& parsed_cookie);
  static bool IsCookiePrefixValid(CookiePrefix prefix,
                                  const GURL& url,
                                  bool secure,
                                  const std::string& domain,
                                  const std::string& path);

  // Returns the effective SameSite mode to apply to this cookie. Depends on the
  // value of the given SameSite attribute and whether the
  // SameSiteByDefaultCookies feature is enabled, as well as the access
  // semantics of the cookie.
  // Note: If you are converting to a different representation of a cookie, you
  // probably want to use SameSite() instead of this method. Otherwise, if you
  // are considering using this method, consider whether you should use
  // IncludeForRequestURL() or IsSetPermittedInContext() instead of doing the
  // SameSite computation yourself.
  CookieEffectiveSameSite GetEffectiveSameSite(
      CookieAccessSemantics access_semantics) const;

  // Returns whether the cookie was created at most |age_threshold| ago.
  bool IsRecentlyCreated(base::TimeDelta age_threshold) const;

  // Returns the cookie's domain, with the leading dot removed, if present.
  std::string DomainWithoutDot() const;

  std::string name_;
  std::string value_;
  std::string domain_;
  std::string path_;
  base::Time creation_date_;
  base::Time expiry_date_;
  base::Time last_access_date_;
  bool secure_;
  bool httponly_;
  CookieSameSite same_site_;
  CookiePriority priority_;
};

// This class represents if a cookie was included or excluded in a cookie get or
// set operation, and if excluded why. It holds a vector of reasons for
// exclusion, where cookie inclusion is represented by the absence of any
// exclusion reasons. Also marks whether a cookie should be warned about, e.g.
// for deprecation or intervention reasons.
// TODO(chlily): Rename/move this to just net::CookieInclusionStatus.
class NET_EXPORT CanonicalCookie::CookieInclusionStatus {
 public:
  // Types of reasons why a cookie might be excluded.
  // If adding a ExclusionReason, please also update the GetDebugString()
  // method.
  enum ExclusionReason {
    EXCLUDE_UNKNOWN_ERROR = 0,

    EXCLUDE_HTTP_ONLY = 1,
    EXCLUDE_SECURE_ONLY = 2,
    EXCLUDE_DOMAIN_MISMATCH = 3,
    EXCLUDE_NOT_ON_PATH = 4,
    EXCLUDE_SAMESITE_STRICT = 5,
    EXCLUDE_SAMESITE_LAX = 6,
    // Reserved 7, was EXCLUDE_SAMESITE_EXTENDED.

    // The following two are used for the SameSiteByDefaultCookies experiment,
    // where if the SameSite attribute is not specified, it will be treated as
    // SameSite=Lax by default.
    EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX = 8,
    // This is used if SameSite=None is specified, but the cookie is not
    // Secure.
    EXCLUDE_SAMESITE_NONE_INSECURE = 9,
    EXCLUDE_USER_PREFERENCES = 10,

    // Statuses specific to setting cookies
    EXCLUDE_FAILURE_TO_STORE = 11,
    EXCLUDE_NONCOOKIEABLE_SCHEME = 12,
    EXCLUDE_OVERWRITE_SECURE = 13,
    EXCLUDE_OVERWRITE_HTTP_ONLY = 14,
    EXCLUDE_INVALID_DOMAIN = 15,
    EXCLUDE_INVALID_PREFIX = 16,

    // This should be kept last.
    NUM_EXCLUSION_REASONS
  };

  // Reason to warn about a cookie. If you add one, please update
  // GetDebugString().
  // TODO(chlily): Do we need to support multiple warning statuses?
  enum WarningReason {
    DO_NOT_WARN = 0,
    // Warn if a cookie with unspecified SameSite attribute is used in a
    // cross-site context.
    WARN_SAMESITE_UNSPECIFIED_CROSS_SITE_CONTEXT,
    // Warn if a cookie with SameSite=None is not Secure.
    WARN_SAMESITE_NONE_INSECURE,
    // Warn if a cookie with unspecified SameSite attribute is defaulted into
    // Lax and is sent on a request with unsafe method, only because it is new
    // enough to activate the Lax-allow-unsafe intervention.
    WARN_SAMESITE_UNSPECIFIED_LAX_ALLOW_UNSAFE,
  };

  // Makes a status that says include.
  CookieInclusionStatus();

  // Makes a status that contains the given exclusion reason and warning.
  explicit CookieInclusionStatus(ExclusionReason reason,
                                 WarningReason warning = DO_NOT_WARN);

  bool operator==(const CookieInclusionStatus& other) const;
  bool operator!=(const CookieInclusionStatus& other) const;

  // Whether the status is to include the cookie, and has no other reasons for
  // exclusion.
  bool IsInclude() const;

  // Whether the given reason for exclusion is present.
  bool HasExclusionReason(ExclusionReason status_type) const;

  // Add an exclusion reason.
  void AddExclusionReason(ExclusionReason status_type);

  // Add all the exclusion reasons given in |other|. If there is a warning in
  // |other| (other than DO_NOT_WARN), also apply that. This could overwrite the
  // existing warning, so set the most important warnings last.
  void AddExclusionReasonsAndWarningIfAny(const CookieInclusionStatus& other);

  // Remove an exclusion reason.
  void RemoveExclusionReason(ExclusionReason reason);

  // Whether the cookie should be warned about.
  bool ShouldWarn() const;

  WarningReason warning() const { return warning_; }
  void set_warning(WarningReason warning) { warning_ = warning; }

  // Used for serialization/deserialization.
  uint32_t exclusion_reasons() const { return exclusion_reasons_; }
  void set_exclusion_reasons(uint32_t exclusion_reasons) {
    exclusion_reasons_ = exclusion_reasons;
  }

  // Get exclusion reason(s) and warning in string format.
  std::string GetDebugString() const;

  // Checks that the underlying bit vector representation doesn't contain any
  // extraneous bits that are not mapped to any enum values. Does not check
  // for reasons which semantically cannot coexist.
  bool IsValid() const;

  // Checks whether the exclusion reasons are exactly the set of exclusion
  // reasons in the vector. (Ignores warnings.)
  bool HasExactlyExclusionReasonsForTesting(
      std::vector<ExclusionReason> reasons) const;

  // Makes a status that contains the given exclusion reasons and warning.
  static CookieInclusionStatus MakeFromReasonsForTesting(
      std::vector<ExclusionReason> reasons,
      WarningReason warning = DO_NOT_WARN);

 private:
  // A bit vector of the applicable exclusion reasons.
  uint32_t exclusion_reasons_ = 0u;

  // Applicable warning reason.
  WarningReason warning_ = DO_NOT_WARN;
};

NET_EXPORT inline std::ostream& operator<<(
    std::ostream& os,
    const CanonicalCookie::CookieInclusionStatus status) {
  return os << status.GetDebugString();
}

// These enable us to pass along a list of excluded cookie with the reason they
// were excluded
struct CookieWithStatus {
  CanonicalCookie cookie;
  CanonicalCookie::CookieInclusionStatus status;
};

// Used to pass excluded cookie information when it's possible that the
// canonical cookie object may not be available.
struct NET_EXPORT CookieAndLineWithStatus {
  CookieAndLineWithStatus();
  CookieAndLineWithStatus(base::Optional<CanonicalCookie> cookie,
                          std::string cookie_string,
                          CanonicalCookie::CookieInclusionStatus status);
  CookieAndLineWithStatus(
      const CookieAndLineWithStatus& cookie_and_line_with_status);

  CookieAndLineWithStatus& operator=(
      const CookieAndLineWithStatus& cookie_and_line_with_status);

  CookieAndLineWithStatus(
      CookieAndLineWithStatus&& cookie_and_line_with_status);

  ~CookieAndLineWithStatus();

  base::Optional<CanonicalCookie> cookie;
  std::string cookie_string;
  CanonicalCookie::CookieInclusionStatus status;
};

}  // namespace net

#endif  // NET_COOKIES_CANONICAL_COOKIE_H_
