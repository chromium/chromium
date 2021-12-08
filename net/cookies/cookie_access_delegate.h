// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_ACCESS_DELEGATE_H_
#define NET_COOKIES_COOKIE_ACCESS_DELEGATE_H_

#include <set>

#include "base/containers/flat_map.h"
#include "net/base/net_export.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/same_party_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace net {

class SchemefulSite;
class SiteForCookies;

class NET_EXPORT CookieAccessDelegate {
 public:
  CookieAccessDelegate();

  CookieAccessDelegate(const CookieAccessDelegate&) = delete;
  CookieAccessDelegate& operator=(const CookieAccessDelegate&) = delete;

  virtual ~CookieAccessDelegate();

  // Returns true if the passed in |url| should be permitted to access secure
  // cookies in addition to URLs that normally do so. Returning false from this
  // method on a URL that would already be treated as secure by default, e.g. an
  // https:// one has no effect.
  virtual bool ShouldTreatUrlAsTrustworthy(const GURL& url) const;

  // Gets the access semantics to apply to |cookie|, based on its domain (i.e.,
  // whether a policy specifies that legacy access semantics should apply).
  virtual CookieAccessSemantics GetAccessSemantics(
      const CanonicalCookie& cookie) const = 0;

  // Returns whether a cookie should be attached regardless of its SameSite
  // value vs the request context.
  virtual bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const SiteForCookies& site_for_cookies) const = 0;

  // Returns the SamePartyContext indicating whether `site` is same-party
  // with `party_context` and `top_frame_site`. If `top_frame_site` is nullptr,
  // then `site` will be checked only against `party_context`.
  virtual SamePartyContext ComputeSamePartyContext(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context) const = 0;

  // Returns whether `site` belongs to a non-singleton First-Party Set.
  virtual bool IsInNontrivialFirstPartySet(
      const net::SchemefulSite& site) const = 0;

  virtual FirstPartySetsContextType ComputeFirstPartySetsContextType(
      const SchemefulSite& site,
      const absl::optional<SchemefulSite>& top_frame_site,
      const std::set<SchemefulSite>& party_context) const = 0;

  // Returns the owner of a `site`'s First-Party Set if `site` is in a
  // non-trivial set. Returns nullopt otherwise.
  virtual absl::optional<net::SchemefulSite> FindFirstPartySetOwner(
      const net::SchemefulSite& site) const = 0;

  // Creates a CookiePartitionKey that takes whether the top-frame site is in a
  // First-Party Set into account. If FPS are not enabled, it returns a cookie
  // partition key that does not take FPS into account.
  //
  // Should always return nullopt if partitioned cookies are disabled or if
  // the NIK has no top-frame site.
  static absl::optional<CookiePartitionKey> CreateCookiePartitionKey(
      const CookieAccessDelegate* delegate,
      const NetworkIsolationKey& network_isolation_key);

  // Converts the CookiePartitionKey's site to its First-Party Set owner if
  // the site is in a nontrivial set.
  static absl::optional<CookiePartitionKey> FirstPartySetifyPartitionKey(
      const CookieAccessDelegate* delegate,
      const CookiePartitionKey& cookie_partition_key);

  // Returns the First-Party Sets.
  virtual base::flat_map<net::SchemefulSite, std::set<net::SchemefulSite>>
  RetrieveFirstPartySets() const = 0;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_ACCESS_DELEGATE_H_
