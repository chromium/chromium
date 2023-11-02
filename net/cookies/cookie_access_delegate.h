// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_ACCESS_DELEGATE_H_
#define NET_COOKIES_COOKIE_ACCESS_DELEGATE_H_

#include <set>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/same_party_context.h"
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

  // Calls `callback` with metadata indicating whether `site` is same-party with
  // `party_context` and `top_frame_site`; and `site`'s owner, if applicable..
  // If `top_frame_site` is nullptr, then `site` will be checked only against
  // `party_context`.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] virtual absl::optional<FirstPartySetMetadata>
  ComputeFirstPartySetMetadataMaybeAsync(
      const net::SchemefulSite& site,
      const net::SchemefulSite* top_frame_site,
      const std::set<net::SchemefulSite>& party_context,
      base::OnceCallback<void(FirstPartySetMetadata)> callback) const = 0;

  // Returns the entries of a set of sites if the sites are in non-trivial sets.
  // If a given site is not in a non-trivial set, the output does not contain a
  // corresponding entry.
  //
  // This may return a result synchronously, or asynchronously invoke `callback`
  // with the result. The callback will be invoked iff the return value is
  // nullopt; i.e. a result will be provided via return value or callback, but
  // not both, and not neither.
  [[nodiscard]] virtual absl::optional<
      base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>>
  FindFirstPartySetEntries(
      const base::flat_set<net::SchemefulSite>& sites,
      base::OnceCallback<
          void(base::flat_map<net::SchemefulSite, net::FirstPartySetEntry>)>
          callback) const = 0;
};

}  // namespace net

#endif  // NET_COOKIES_COOKIE_ACCESS_DELEGATE_H_
