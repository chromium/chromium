// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_BASE_H_
#define NET_COOKIES_COOKIE_BASE_H_

#include <optional>
#include <string>
#include <tuple>

#include "net/base/net_export.h"
#include "net/cookies/cookie_access_params.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key.h"

class GURL;

namespace net {

// A base class for cookies and cookie-like objects. Encapsulates logic for
// determining whether a cookie could be sent/set, based on its attributes and
// the request context.
class NET_EXPORT CookieBase {
 public:
  // StrictlyUniqueCookieKey always populates the cookie's source scheme and
  // source port.
  using StrictlyUniqueCookieKey = std::tuple<std::optional<CookiePartitionKey>,
                                             /*name=*/std::string,
                                             /*domain=*/std::string,
                                             /*path=*/std::string,
                                             CookieSourceScheme,
                                             /*source_port=*/int>;

  // Conditionally populates the source scheme and source port depending on the
  // state of their associated feature.
  using UniqueCookieKey = std::tuple<std::optional<CookiePartitionKey>,
                                     /*name=*/std::string,
                                     /*domain=*/std::string,
                                     /*path=*/std::string,
                                     std::optional<CookieSourceScheme>,
                                     /*source_port=*/std::optional<int>>;

  // Same as UniqueCookieKey but for use with Domain cookies, which do not
  // consider the source_port.
  using UniqueDomainCookieKey = std::tuple<std::optional<CookiePartitionKey>,
                                           /*name=*/std::string,
                                           /*domain=*/std::string,
                                           /*path=*/std::string,
                                           std::optional<CookieSourceScheme>>;

  // Returns if the cookie should be included (and if not, why) for the given
  // request |url| using the CookieInclusionStatus enum. HTTP only cookies can
  // be filter by using appropriate cookie |options|.
  //
  // PLEASE NOTE that this method does not check whether a cookie is expired or
  // not!
  CookieAccessResult IncludeForRequestURL(
      const GURL& url,
      const CookieOptions& options,
      const CookieAccessParams& params) const;

  // Returns if the cookie with given attributes can be set in context described
  // by |options| and |params|, and if no, describes why.
  //
  // |cookie_access_result| is an optional input status, to allow for status
  // chaining from callers. It helps callers provide the status of a
  // cookie that may have warnings associated with it.
  CookieAccessResult IsSetPermittedInContext(
      const GURL& source_url,
      const CookieOptions& options,
      const CookieAccessParams& params,
      const std::vector<std::string>& cookieable_schemes,
      const std::optional<CookieAccessResult>& cookie_access_result =
          std::nullopt) const;

  // Returns true if the given |url_path| path-matches this cookie's cookie-path
  // as described in section 5.1.4 in RFC 6265. This returns true if |path_| and
  // |url_path| are identical, or if |url_path| is a subdirectory of |path_|.
  bool IsOnPath(const std::string& url_path) const;

  // This returns true if this cookie's |domain_| indicates that it can be
  // accessed by |host|.
  //
  // In the case where |domain_| has no leading dot, this is a host cookie and
  // will only domain match if |host| is identical to |domain_|.
  //
  // In the case where |domain_| has a leading dot, this is a domain cookie. It
  // will match |host| if |domain_| is a suffix of |host|, or if |domain_| is
  // exactly equal to |host| plus a leading dot.
  //
  // Note that this isn't quite the same as the "domain-match" algorithm in RFC
  // 6265bis, since our implementation uses the presence of a leading dot in the
  // |domain_| string in place of the spec's host-only-flag. That is, if
  // |domain_| has no leading dot, then we only consider it matching if |host|
  // is identical (which reflects the intended behavior when the cookie has a
  // host-only-flag), whereas the RFC also treats them as domain-matching if
  // |domain_| is a subdomain of |host|.
  bool IsDomainMatch(const std::string& host) const;

  const std::string& Name() const { return name_; }
  // We represent the cookie's host-only-flag as the absence of a leading dot in
  // Domain(). See IsDomainCookie() and IsHostCookie() below.
  // If you want the "cookie's domain" as described in RFC 6265bis, use
  // DomainWithoutDot().
  const std::string& Domain() const { return domain_; }
  const std::string& Path() const { return path_; }
  const base::Time& CreationDate() const { return creation_date_; }
  bool SecureAttribute() const { return secure_; }
  bool IsHttpOnly() const { return httponly_; }
  CookieSameSite SameSite() const { return same_site_; }

  // Returns true if this cookie can only be accessed in a secure context.
  bool IsSecure() const;

  bool IsPartitioned() const { return partition_key_.has_value(); }
  const std::optional<CookiePartitionKey>& PartitionKey() const {
    return partition_key_;
  }

  // Returns whether this cookie is Partitioned and its partition key matches a
  // a same-site context by checking if the cookies domain site is the same as
  // the partition key's site.
  // This function should not be used for third-party cookie blocking
  // enforcement-related decisions. That logic should rely on `IsPartitioned`.
  // These functions are for recording metrics about partitioned cookie usage.
  // Returns false if the cookie has no partition key.
  bool IsFirstPartyPartitioned() const;

  // Returns whether the cookie is partitioned in a third-party context.
  // This function should not be used for third-party cookie blocking
  // enforcement-related decisions. That logic should rely on `IsPartitioned`.
  // These functions are for recording metrics about partitioned cookie usage.
  // Returns false if the cookie has no partition key.
  bool IsThirdPartyPartitioned() const;

  // Returns an enum indicating the scheme of the origin that
  // set this cookie. This is not part of the cookie spec but is being used to
  // collect metrics for a potential change to the cookie spec
  // (https://tools.ietf.org/html/draft-west-cookie-incrementalism-01#section-3.4)
  CookieSourceScheme SourceScheme() const { return source_scheme_; }
  // Returns the port of the origin that originally set this cookie (the
  // source port). This is not part of the cookie spec but is being used to
  // collect metrics for a potential change to the cookie spec.
  int SourcePort() const { return source_port_; }

  bool IsDomainCookie() const { return !domain_.empty() && domain_[0] == '.'; }
  bool IsHostCookie() const { return !IsDomainCookie(); }

  // Returns the cookie's domain, with the leading dot removed, if present.
  // This corresponds to the "cookie's domain" as described in RFC 6265bis.
  std::string DomainWithoutDot() const;

  StrictlyUniqueCookieKey StrictlyUniqueKey() const {
    return std::make_tuple(partition_key_, name_, domain_, path_,
                           source_scheme_, source_port_);
  }

  // Returns a key such that two cookies with the same UniqueKey() are
  // guaranteed to be equivalent in the sense of IsEquivalent().
  // The `partition_key_` field will always be nullopt when partitioned cookies
  // are not enabled.
  // The source_scheme and source_port fields depend on whether or not their
  // associated features are enabled.
  UniqueCookieKey UniqueKey() const;

  // Same as UniqueKey() except it does not contain a source_port field. For use
  // with Domain cookies, which do not consider the source_port.
  UniqueDomainCookieKey UniqueDomainKey() const;

  void SetSourceScheme(CookieSourceScheme source_scheme) {
    source_scheme_ = source_scheme;
  }

  // Set the source port value. Performs a range check and sets the port to
  // url::PORT_INVALID if value isn't in [0,65535] or url::PORT_UNSPECIFIED.
  void SetSourcePort(int port);

  void SetCreationDate(const base::Time& date) { creation_date_ = date; }

 protected:
  CookieBase();
  CookieBase(const CookieBase& other);
  CookieBase(CookieBase&& other);
  CookieBase& operator=(const CookieBase& other);
  CookieBase& operator=(CookieBase&& other);
  virtual ~CookieBase();

  CookieBase(std::string name,
             std::string domain,
             std::string path,
             base::Time creation,
             bool secure,
             bool httponly,
             CookieSameSite same_site,
             std::optional<CookiePartitionKey> partition_key,
             CookieSourceScheme source_scheme = CookieSourceScheme::kUnset,
             int source_port = url::PORT_UNSPECIFIED);

  // Returns the effective SameSite mode to apply to this cookie. Depends on the
  // value of the given SameSite attribute and the access semantics of the
  // cookie.
  // Note: If you are converting to a different representation of a cookie, you
  // probably want to use SameSite() instead of this method. Otherwise, if you
  // are considering using this method, consider whether you should use
  // IncludeForRequestURL() or IsSetPermittedInContext() instead of doing the
  // SameSite computation yourself.
  CookieEffectiveSameSite GetEffectiveSameSite(
      CookieAccessSemantics access_semantics) const;

  // Returns the threshold age for lax-allow-unsafe behavior, below which the
  // effective SameSite behavior for a cookie that does not specify SameSite is
  // lax-allow-unsafe, and above which the effective SameSite is just lax.
  // Lax-allow-unsafe behavior (a.k.a. Lax+POST) is a temporary mitigation for
  // compatibility reasons that allows a cookie which doesn't specify SameSite
  // to still be sent on non-safe requests like POST requests for a short amount
  // of time after creation, despite the default enforcement for most (i.e.
  // older) SameSite-unspecified cookies being Lax. Implementations should
  // override this method if they want to enable Lax-allow-unsafe behavior; by
  // default, this method returns base::TimeDelta::Min(), i.e. no cookies will
  // ever be lax-allow-unsafe.
  virtual base::TimeDelta GetLaxAllowUnsafeThresholdAge() const;

  // Returns whether the cookie was created at most |age_threshold| ago.
  bool IsRecentlyCreated(base::TimeDelta age_threshold) const;

  // Checks if `port` is within [0,65535] or url::PORT_UNSPECIFIED. Returns
  // `port` if so and url::PORT_INVALID otherwise.
  static int ValidateAndAdjustSourcePort(int port);

 private:
  // Allows subclasses to add custom logic for e.g. logging metrics. Called
  // after inclusion has been determined for the respective access.
  virtual void PostIncludeForRequestURL(
      const CookieAccessResult& access_result,
      const CookieOptions& options_used,
      CookieOptions::SameSiteCookieContext::ContextType
          cookie_inclusion_context_used) const {}
  virtual void PostIsSetPermittedInContext(
      const CookieAccessResult& access_result,
      const CookieOptions& options_used) const {}

  // Keep defaults here in sync with
  // services/network/public/interfaces/cookie_manager.mojom.
  std::string name_;
  std::string domain_;
  std::string path_;
  base::Time creation_date_;
  bool secure_{false};
  bool httponly_{false};
  CookieSameSite same_site_{CookieSameSite::NO_RESTRICTION};
  // This will be std::nullopt for all cookies not set with the Partitioned
  // attribute or without a nonce. If the value is non-null, then the cookie
  // will only be delivered when the top-frame site matches the partition key
  // and the nonce (if present). If the partition key is non-null and opaque,
  // this means the Partitioned cookie was created on an opaque origin or with
  // a nonce.
  std::optional<CookiePartitionKey> partition_key_;
  CookieSourceScheme source_scheme_{CookieSourceScheme::kUnset};
  // This can be [0,65535], PORT_UNSPECIFIED, or PORT_INVALID.
  // PORT_UNSPECIFIED is used for cookies which already existed in the cookie
  // store prior to this change and therefore their port is unknown.
  // PORT_INVALID is an error for when an out of range port is provided.
  int source_port_{url::PORT_UNSPECIFIED};
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_BASE_H_
