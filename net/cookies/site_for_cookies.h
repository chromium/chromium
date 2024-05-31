// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_SITE_FOR_COOKIES_H_
#define NET_COOKIES_SITE_FOR_COOKIES_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

// Represents which origins are to be considered same-site for a given
// context (e.g. frame). There may be none.
//
// The currently implemented policy ("schemeful") is that
// two valid URLs would be considered same-site if either:
// 1) They both have compatible schemes along with non-empty and equal
//    registrable domains or hostnames/IPs. Two schemes are compatible if they
//    are either equal, or are both in {http, ws}, or are both in {https, wss}.
//    E.x. "https://example.com" and "wss://example.com"
// 2) They both have empty hostnames and exactly equal schemes. E.x. "file://"
// and "file://"
//
// With the SchemefulSameSite feature disabled the policy ("schemeless") is that
// two valid URLs would be considered the same site if either: 1) They both have
// non-empty and equal registrable domains or hostnames/IPs. E.x. "example.com"
// and "example.com" 2) They both have empty hostnames and equal schemes. E.x.
// "file://" and "file://"
//
//
// Invalid URLs are never same-site to anything.
class NET_EXPORT SiteForCookies {
 public:
  // Matches nothing.
  SiteForCookies();

  SiteForCookies(const SiteForCookies& other);
  SiteForCookies(SiteForCookies&& other);

  explicit SiteForCookies(const SchemefulSite& schemeful_site);

  ~SiteForCookies();

  SiteForCookies& operator=(const SiteForCookies& other);
  SiteForCookies& operator=(SiteForCookies&& other);

  // Tries to construct an instance from (potentially untrusted) values of
  // site() and schemefully_same() that got received over an RPC.
  //
  // Returns whether successful or not. Doesn't touch `*out` if false is
  // returned.  This returning `true` does not mean that whoever sent the values
  // did not lie, merely that they are well-formed.
  static bool FromWire(const SchemefulSite& site,
                       bool schemefully_same,
                       SiteForCookies* out);

  // If the origin is opaque, returns SiteForCookies that matches nothing.
  //
  // If it's not opaque, returns one that matches URLs which are considered to
  // be same-party as URLs from `origin`.
  static SiteForCookies FromOrigin(const url::Origin& origin);

  // Equivalent to FromOrigin(url::Origin::Create(url)).
  static SiteForCookies FromUrl(const GURL& url);

  // Returns a string with the values of the member variables.
  // `schemefully_same` being false does not change the output.
  std::string ToDebugString() const;

  // Returns true if `url` should be considered first-party to the context
  // `this` represents.
  bool IsFirstParty(const GURL& url) const;

  // Don't use this function unless you know what you're doing, if you're unsure
  // you probably want IsFirstParty().
  //
  // If `compute_schemefully` is true this function will return true if `url`
  // should be considered first-party to the context `this` represents when the
  // compatibility of the schemes are taken into account.
  //
  // If `compute_schemefully` is false this function will return true if `url`
  // should be considered first-party to the context `this` represents when the
  // compatibility of the scheme are not taken into account. Note that schemes
  // are still compared for exact equality if neither `this` nor `url` have a
  // registered domain.
  bool IsFirstPartyWithSchemefulMode(const GURL& url,
                                     bool compute_schemefully) const;

  // Returns true if `other.IsFirstParty()` is true for exactly the same URLs
  // as `this->IsFirstParty` (potentially none). Two SFCs are also considered
  // equivalent if neither ever returns true for `IsFirstParty()`. I.e., both
  // are null.
  bool IsEquivalent(const SiteForCookies& other) const;

  // Compares a "candidate" SFC, `this`, with an origin (represented as a
  // SchemefulSite) from the frame tree and revises `this` to be null as
  // required.
  //
  // This method is used when a sub-frame needs to have its SiteForCookies
  // computed, which is dependent on all of its ancestors' origins (`other`). If
  // its or any of its ancestors' frame's origins are not first-party with the
  // top-level origin then this frame's SFC should be null. Otherwise, if it and
  // all its ancestors are first-party with the top-level frame, the frame's
  // SFC is the same as the top-level.
  //
  // This computation gets a bit tricky when considering "Schemeful Same-Site"
  // as we don't know ahead of time if `this` is going to be used for a
  // schemeful or schemeless computation later down the line, so we need to
  // be careful to not completely nullify the entire SFC just because an `other`
  // isn't schemefully first-party. If the computation is schemelessly
  // first-party but *not schemefully* first-party then only the
  // `schemefully_same_` flag will be cleared (which informs other functions to
  // treat `this` as null for schemeful purposes). If the computation is not
  // schemelessly first-party then `this` will have an opaque `site_` which
  // completely nullifies it.
  //
  // This function should be called on all ancestors up to the top-level frame
  // unless it returns false in which case `this` has been completely nullified
  // and the caller may stop early.
  //
  // (This function's return value is the same as IsEquivalent() when "Schemeful
  // Same-Site" is disabled.)
  bool CompareWithFrameTreeSiteAndRevise(const SchemefulSite& other);

  // Converts an Origin into a SchemefulSite and then calls
  //`CompareWithFrameTreeSiteAndRevise(SchemefulSite)`
  //
  // If possible, prefer `CompareWithFrameTreeSiteAndRevise(SchemefulSite)`
  // for performance reasons.
  bool CompareWithFrameTreeOriginAndRevise(const url::Origin& other);

  // Returns a URL that's first party to this SiteForCookies (an empty URL if
  // none) --- that is, it has the property that
  // site_for_cookies.IsEquivalent(
  //     SiteForCookies::FromUrl(site_for_cookies.RepresentativeUrl()));
  //
  // The convention used here (empty for nothing) is equivalent to that
  // used before SiteForCookies existed as a type; this method is mostly
  // meant to help incrementally migrate towards the type. New code probably
  // should not need this.
  GURL RepresentativeUrl() const;

  const SchemefulSite& site() const { return site_; }

  // Guaranteed to be lowercase.
  const std::string& scheme() const { return site_.site_as_origin_.scheme(); }

  const std::string& registrable_domain() const {
    return site_.site_as_origin_.host();
  }

  // Used for serialization/deserialization. This value is irrelevant if
  // site().opaque() is true.
  bool schemefully_same() const { return schemefully_same_; }

  void SetSchemefullySameForTesting(bool schemefully_same) {
    schemefully_same_ = schemefully_same;
  }

  // Returns true if this SiteForCookies matches nothing.
  // If the SchemefulSameSite feature is enabled then !schemefully_same_ causes
  // this function to return true.
  bool IsNull() const;

  // Allows SiteForCookies to be used as a key in STL (for example, a std::set
  // or std::map).
  NET_EXPORT friend bool operator<(const SiteForCookies& lhs,
                                   const SiteForCookies& rhs);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteForCookiesTest, SameScheme);
  FRIEND_TEST_ALL_PREFIXES(SiteForCookiesTest, SameSchemeOpaque);

  bool IsSchemefullyFirstParty(const GURL& url) const;

  bool IsSchemelesslyFirstParty(const GURL& url) const;

  // Sets the schemefully_same_ flag to false if other`'s scheme is
  // cross-scheme to `this`. Schemes are considered cross-scheme if they're not
  // compatible. Two schemes are compatible if they are either equal, or are
  // both in {http, ws}, or are both in {https, wss}.
  void MarkIfCrossScheme(const SchemefulSite& other);

  // Represents the scheme and registrable domain of the site. The scheme should
  // not be a WebSocket scheme; instead, ws is normalized to http, and
  // wss is normalized to https upon construction.
  SchemefulSite site_;

  // Used to indicate if the SiteForCookies would be the same if computed
  // schemefully. A schemeful computation means to take the scheme as well as
  // the registrable_domain into account when determining same-siteness.
  // See the class-level comment for more details on schemeless vs schemeful.
  //
  // True means to treat `this` as-is while false means that `this` should be
  // treated as if it matches nothing i.e. IsNull() returns true.
  //
  // This value is important in the case where the SiteForCookies is being used
  // to assess the first-partyness of a sub-frame in a document.
  //
  // For a SiteForCookies with !site_.opaque() this value starts as true and
  // will only go false via MarkIfCrossScheme(), otherwise this value is
  // irrelevant (For tests this value can also be modified by
  // SetSchemefullySameForTesting()).
  bool schemefully_same_ = false;
};

}  // namespace net

#endif  // NET_COOKIES_SITE_FOR_COOKIES_H_
