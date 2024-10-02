// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_CANONICAL_COOKIE_H_
#define NET_COOKIES_CANONICAL_COOKIE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "net/base/features.h"
#include "net/base/net_export.h"
#include "net/cookies/cookie_access_params.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_base.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key.h"
#include "url/third_party/mozilla/url_parse.h"

class GURL;

namespace net {

class ParsedCookie;
class CanonicalCookie;

struct CookieWithAccessResult;
struct CookieAndLineWithAccessResult;

using CookieList = std::vector<CanonicalCookie>;
using CookieAndLineAccessResultList =
    std::vector<CookieAndLineWithAccessResult>;
using CookieAccessResultList = std::vector<CookieWithAccessResult>;

// Represents a real/concrete cookie, which may be sent on requests or set by a
// response if the request context and attributes allow it.
class NET_EXPORT CanonicalCookie : public CookieBase {
 public:
  CanonicalCookie();
  CanonicalCookie(const CanonicalCookie& other);
  CanonicalCookie(CanonicalCookie&& other);
  CanonicalCookie& operator=(const CanonicalCookie& other);
  CanonicalCookie& operator=(CanonicalCookie&& other);
  ~CanonicalCookie() override;

  // This constructor does not validate or canonicalize their inputs;
  // the resulting CanonicalCookies should not be relied on to be canonical
  // unless the caller has done appropriate validation and canonicalization
  // themselves.
  //
  // NOTE: Prefer using Create, CreateSanitizedCookie, or FromStorage (depending
  // on the use case) over directly using this constructor.
  //
  // NOTE: Do not add any defaults to this constructor, we want every caller to
  // understand and choose their inputs.
  CanonicalCookie(base::PassKey<CanonicalCookie>,
                  std::string name,
                  std::string value,
                  std::string domain,
                  std::string path,
                  base::Time creation,
                  base::Time expiration,
                  base::Time last_access,
                  base::Time last_update,
                  bool secure,
                  bool httponly,
                  CookieSameSite same_site,
                  CookiePriority priority,
                  std::optional<CookiePartitionKey> partition_key,
                  CookieSourceScheme scheme_secure,
                  int source_port,
                  CookieSourceType source_type);

  // Creates a new `CanonicalCookie` from the `cookie_line` and the
  // `creation_time`.  Canonicalizes inputs.  May return nullptr if
  // an attribute value is invalid.  `url` must be valid.  `creation_time` may
  // not be null. Sets optional `status` to the relevant CookieInclusionStatus
  // if provided.  `server_time` indicates what the server sending us the Cookie
  // thought the current time was when the cookie was produced.  This is used to
  // adjust for clock skew between server and host.
  //
  // SameSite and HttpOnly related parameters are not checked here,
  // so creation of CanonicalCookies with e.g. SameSite=Strict from a cross-site
  // context is allowed. Create() also does not check whether `url` has a secure
  // scheme if attempting to create a Secure cookie. The Secure, SameSite, and
  // HttpOnly related parameters should be checked when setting the cookie in
  // the CookieStore.
  //
  // The partition_key argument only needs to be present if the cookie line
  // contains the Partitioned attribute. If the cookie line will never contain
  // that attribute, you should use std::nullopt to indicate you intend to
  // always create an unpartitioned cookie. If partition_key has a value but the
  // cookie line does not contain the Partitioned attribute, the resulting
  // cookie will be unpartitioned. If the partition_key is null, then the cookie
  // will be unpartitioned even when the cookie line has the Partitioned
  // attribute.
  //
  // If a cookie is returned, `cookie->IsCanonical()` will be true.
  //
  // NOTE: Do not add any defaults to this constructor, we want every caller to
  // understand and choose their inputs.
  static std::unique_ptr<CanonicalCookie> Create(
      const GURL& url,
      const std::string& cookie_line,
      const base::Time& creation_time,
      std::optional<base::Time> server_time,
      std::optional<CookiePartitionKey> cookie_partition_key,
      CookieSourceType source_type,
      CookieInclusionStatus* status);

  // Create a canonical cookie based on sanitizing the passed inputs in the
  // context of the passed URL.  Returns a null unique pointer if the inputs
  // cannot be sanitized.  If `status` is provided it will have any relevant
  // CookieInclusionStatus rejection reasons set. If a cookie is created,
  // `cookie->IsCanonical()` will be true.
  //
  // NOTE: Do not add any defaults to this constructor, we want every caller to
  // understand and choose their inputs.
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
      CookiePriority priority,
      std::optional<CookiePartitionKey> partition_key,
      CookieInclusionStatus* status);

  // FromStorage is a factory method which is meant for creating a new
  // CanonicalCookie using properties of a previously existing cookie
  // that was already ingested into the cookie store.
  // This should NOT be used to create a new CanonicalCookie that was not
  // already in the store.
  // Returns nullptr if the resulting cookie is not canonical,
  // i.e. cc->IsCanonical() returns false.
  //
  // NOTE: Do not add any defaults to this constructor, we want every caller to
  // understand and choose their inputs.
  static std::unique_ptr<CanonicalCookie> FromStorage(
      std::string name,
      std::string value,
      std::string domain,
      std::string path,
      base::Time creation,
      base::Time expiration,
      base::Time last_access,
      base::Time last_update,
      bool secure,
      bool httponly,
      CookieSameSite same_site,
      CookiePriority priority,
      std::optional<CookiePartitionKey> partition_key,
      CookieSourceScheme source_scheme,
      int source_port,
      CookieSourceType source_type);

  // Create a CanonicalCookie that is not guaranteed to actually be Canonical
  // for tests. Use this only if you want to bypass parameter validation to
  // create a cookie that otherwise shouldn't be possible to store.
  static std::unique_ptr<CanonicalCookie> CreateUnsafeCookieForTesting(
      const std::string& name,
      const std::string& value,
      const std::string& domain,
      const std::string& path,
      const base::Time& creation,
      const base::Time& expiration,
      const base::Time& last_access,
      const base::Time& last_update,
      bool secure,
      bool httponly,
      CookieSameSite same_site,
      CookiePriority priority,
      std::optional<CookiePartitionKey> partition_key = std::nullopt,
      CookieSourceScheme scheme_secure = CookieSourceScheme::kUnset,
      int source_port = url::PORT_UNSPECIFIED,
      CookieSourceType source_type = CookieSourceType::kUnknown);

  // Like Create but with some more friendly defaults for use in tests.
  static std::unique_ptr<CanonicalCookie> CreateForTesting(
      const GURL& url,
      const std::string& cookie_line,
      const base::Time& creation_time,
      std::optional<base::Time> server_time = std::nullopt,
      std::optional<CookiePartitionKey> cookie_partition_key = std::nullopt,
      CookieSourceType source_type = CookieSourceType::kUnknown,
      CookieInclusionStatus* status = nullptr);

  bool operator<(const CanonicalCookie& other) const {
    // Use the cookie properties that uniquely identify a cookie to determine
    // ordering.
    return UniqueKey() < other.UniqueKey();
  }

  bool operator==(const CanonicalCookie& other) const {
    return IsEquivalent(other);
  }

  // See CookieBase for other accessors.
  const std::string& Value() const { return value_; }
  const base::Time& ExpiryDate() const { return expiry_date_; }
  const base::Time& LastAccessDate() const { return last_access_date_; }
  const base::Time& LastUpdateDate() const { return last_update_date_; }
  bool IsPersistent() const { return !expiry_date_.is_null(); }
  CookiePriority Priority() const { return priority_; }
  CookieSourceType SourceType() const { return source_type_; }

  bool IsExpired(const base::Time& current) const {
    return !expiry_date_.is_null() && current >= expiry_date_;
  }

  // Are the cookies considered equivalent in the eyes of RFC 2965.
  // The RFC says that name must match (case-sensitive), domain must
  // match (case insensitive), and path must match (case sensitive).
  // For the case insensitive domain compare, we rely on the domain
  // having been canonicalized (in
  // GetCookieDomainWithString->CanonicalizeHost).
  // If partitioned cookies are enabled, then we check the cookies have the same
  // partition key in addition to the checks in RFC 2965.
  //
  // To support origin-bound cookies the check will also include the source
  // scheme and/or port depending on the state of the associated feature.
  // Additionally, domain cookies get a slightly different check which does not
  // include the source port.
  bool IsEquivalent(const CanonicalCookie& ecc) const {
    // It seems like it would make sense to take secure, httponly, and samesite
    // into account, but the RFC doesn't specify this.
    // NOTE: Keep this logic in-sync with TrimDuplicateCookiesForKey().

    // A host cookie will never match a domain cookie or vice-versa, this is
    // because the "host-only-flag" is encoded within the `domain` field of the
    // respective keys. So we don't need to explicitly check if ecc is also host
    // or domain.
    if (IsHostCookie()) {
      return UniqueKey() == ecc.UniqueKey();
    }
    // Is domain cookie
    return UniqueDomainKey() == ecc.UniqueDomainKey();
  }

  // Checks a looser set of equivalency rules than 'IsEquivalent()' in order
  // to support the stricter 'Secure' behaviors specified in Step 12 of
  // https://tools.ietf.org/html/draft-ietf-httpbis-rfc6265bis-05#section-5.4
  // which originated from the proposal in
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-alone#section-3
  //
  // Returns 'true' if this cookie's name matches |secure_cookie|, and this
  // cookie is a domain-match for |secure_cookie| (or vice versa), and
  // |secure_cookie|'s path is "on" this cookie's path (as per 'IsOnPath()').
  // If partitioned cookies are enabled, it also checks that the cookie has
  // the same partition key as |secure_cookie|.
  //
  // Note that while the domain-match cuts both ways (e.g. 'example.com'
  // matches 'www.example.com' in either direction), the path-match is
  // unidirectional (e.g. '/login/en' matches '/login' and '/', but
  // '/login' and '/' do not match '/login/en').
  //
  // Conceptually:
  // If new_cookie.IsEquivalentForSecureCookieMatching(secure_cookie) is true,
  // this means that new_cookie would "shadow" secure_cookie: they would would
  // be indistinguishable when serialized into a Cookie header. This is
  // important because, if an attacker is attempting to set new_cookie, it
  // should not be allowed to mislead the server into using new_cookie's value
  // instead of secure_cookie's.
  //
  // The reason for the asymmetric path comparison ("cookie1=bad; path=/a/b"
  // from an insecure source is not allowed if "cookie1=good; secure; path=/a"
  // exists, but "cookie2=bad; path=/a" from an insecure source is allowed if
  // "cookie2=good; secure; path=/a/b" exists) is because cookies in the Cookie
  // header are serialized with longer path first. (See CookieSorter in
  // cookie_monster.cc.) That is, they would be serialized as "Cookie:
  // cookie1=bad; cookie1=good" in one case, and "Cookie: cookie2=good;
  // cookie2=bad" in the other case. The first scenario is not allowed because
  // the attacker injects the bad value, whereas the second scenario is ok
  // because the good value is still listed first.
  bool IsEquivalentForSecureCookieMatching(
      const CanonicalCookie& secure_cookie) const;

  // Returns true if the |other| cookie's data members (instance variables)
  // match, for comparing cookies in collections.
  bool HasEquivalentDataMembers(const CanonicalCookie& other) const;

  void SetLastAccessDate(const base::Time& date) {
    last_access_date_ = date;
  }

  std::string DebugString() const;

  // Returns a "null" time if expiration was unspecified or invalid.
  static base::Time ParseExpiration(const ParsedCookie& pc,
                                    const base::Time& current,
                                    const base::Time& server_time);

  // Per rfc6265bis the maximum expiry date is no further than 400 days in the
  // future.
  static base::Time ValidateAndAdjustExpiryDate(const base::Time& expiry_date,
                                                const base::Time& creation_date,
                                                net::CookieSourceScheme scheme);

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

  // Return whether this object is a valid CanonicalCookie() when retrieving the
  // cookie from the persistent store. Cookie that exist in the persistent store
  // may have been created before more recent changes to the definition of
  // "canonical". To ease the transition to the new definitions, and to prevent
  // users from having their cookies deleted, this function supports the older
  // definition of canonical. This function is intended to be temporary because
  // as the number of older cookies (which are non-compliant with the newer
  // definition of canonical) decay toward zero it can eventually be replaced
  // by `IsCanonical()` to enforce the newer definition of canonical.
  //
  // A cookie is considered canonical by this function if-and-only-if:
  // * It is considered canonical by IsCanonical()
  // * TODO(crbug.com/40787717): Add exceptions once IsCanonical() starts
  // enforcing them.
  bool IsCanonicalForFromStorage() const;

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

  // Same as above but takes a CookieAccessResultList
  // (ignores the access result).
  static std::string BuildCookieLine(const CookieAccessResultList& cookies);

  // Takes a single CanonicalCookie and returns a cookie line containing the
  // attributes of |cookie| formatted like a http set cookie header.
  // (e.g. "cookie1=value1; domain=abc.com; path=/; secure").
  static std::string BuildCookieAttributesLine(const CanonicalCookie& cookie);

 private:
  FRIEND_TEST_ALL_PREFIXES(CanonicalCookieTest,
                           TestGetAndAdjustPortForTrustworthyUrls);
  FRIEND_TEST_ALL_PREFIXES(CanonicalCookieTest, TestHasHiddenPrefixName);

  // Records histograms to measure how often cookie prefixes appear in
  // the wild and how often they would be blocked.
  static void RecordCookiePrefixMetrics(CookiePrefix prefix);

  // Returns the appropriate port value for the given `source_url` depending on
  // if the url is considered trustworthy or not.
  //
  // This function normally returns source_url.EffectiveIntPort(), but it can
  // return a different port value if:
  // * `source_url`'s scheme isn't cryptographically secure
  // * `url_is_trustworthy` is true
  // * `source_url`'s port is the default port for the scheme i.e.: 80
  // If all these conditions are true then the returned value will be 443 to
  // indicate that we're treating `source_url` as if it was secure.
  static int GetAndAdjustPortForTrustworthyUrls(const GURL& source_url,
                                                bool url_is_trustworthy);

  // Checks for values that could be misinterpreted as a cookie name prefix.
  static bool HasHiddenPrefixName(std::string_view cookie_value);

  // CookieBase:
  base::TimeDelta GetLaxAllowUnsafeThresholdAge() const override;
  void PostIncludeForRequestURL(
      const CookieAccessResult& access_result,
      const CookieOptions& options_used,
      CookieOptions::SameSiteCookieContext::ContextType
          cookie_inclusion_context_used) const override;
  void PostIsSetPermittedInContext(
      const CookieAccessResult& access_result,
      const CookieOptions& options_used) const override;

  // Keep defaults here in sync with
  // services/network/public/interfaces/cookie_manager.mojom.
  // These are the fields specific to CanonicalCookie. See CookieBase for other
  // data fields.
  // If adding more data fields, please also adjust GetAllDataMembersAsTuple().
  std::string value_;
  base::Time expiry_date_;
  base::Time last_access_date_;
  base::Time last_update_date_;
  CookiePriority priority_{COOKIE_PRIORITY_MEDIUM};
  CookieSourceType source_type_{CookieSourceType::kUnknown};
};

// Used to pass excluded cookie information when it's possible that the
// canonical cookie object may not be available.
struct NET_EXPORT CookieAndLineWithAccessResult {
  CookieAndLineWithAccessResult();
  CookieAndLineWithAccessResult(std::optional<CanonicalCookie> cookie,
                                std::string cookie_string,
                                CookieAccessResult access_result);
  CookieAndLineWithAccessResult(
      const CookieAndLineWithAccessResult& cookie_and_line_with_access_result);

  CookieAndLineWithAccessResult& operator=(
      const CookieAndLineWithAccessResult& cookie_and_line_with_access_result);

  CookieAndLineWithAccessResult(
      CookieAndLineWithAccessResult&& cookie_and_line_with_access_result);

  ~CookieAndLineWithAccessResult();

  std::optional<CanonicalCookie> cookie;
  std::string cookie_string;
  CookieAccessResult access_result;
};

struct CookieWithAccessResult {
  CanonicalCookie cookie;
  CookieAccessResult access_result;
};

// Provided to allow gtest to create more helpful error messages, instead of
// printing hex.
inline void PrintTo(const CanonicalCookie& cc, std::ostream* os) {
  *os << "{ name=" << cc.Name() << ", value=" << cc.Value() << " }";
}
inline void PrintTo(const CookieWithAccessResult& cwar, std::ostream* os) {
  *os << "{ ";
  PrintTo(cwar.cookie, os);
  *os << ", ";
  PrintTo(cwar.access_result, os);
  *os << " }";
}
inline void PrintTo(const CookieAndLineWithAccessResult& calwar,
                    std::ostream* os) {
  *os << "{ ";
  if (calwar.cookie) {
    PrintTo(*calwar.cookie, os);
  } else {
    *os << "nullopt";
  }
  *os << ", " << calwar.cookie_string << ", ";
  PrintTo(calwar.access_result, os);
  *os << " }";
}

}  // namespace net

#endif  // NET_COOKIES_CANONICAL_COOKIE_H_
