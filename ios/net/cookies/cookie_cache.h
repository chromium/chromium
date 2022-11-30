// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_CACHE_H_
#define IOS_NET_COOKIES_COOKIE_CACHE_H_

#include <map>
#include <set>
#include <string>
#include <utility>

#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace net {

// CookieCache is a specialized cache for storing the set of cookies with a
// specified name that would be sent with requests for a given URL. It only
// provides one operation, Update(), which updates the set of cookies for a
// (url, name) pair and returns whether the new set for that (url, name) pair is
// different from the old set.
class CookieCache {
 public:
  CookieCache();

  CookieCache(const CookieCache&) = delete;
  CookieCache& operator=(const CookieCache&) = delete;

  ~CookieCache();

  // Update the cookie cache with cookies named |name| that would be sent for a
  // request to |url|. The new vector of cookies overwrite the existing cookies
  // stored for the specified (url, name) pair.
  //
  // Returns true if the new set is different from the old set, i.e.:
  //  * any cookie in |new_cookies| is not present in the cache
  //  * any cookie in the cache is not present in |new_cookies|
  //  * any cookie in both |new_cookies| and the cache has changed value
  // Returns false otherwise.
  //
  // |out_removed_cookies|, if not NULL, will be populated with the cookies that
  // were removed.
  // |out_changed_cookies|, if not NULL, will be populated with the cookies that
  // were added.
  bool Update(const GURL& url,
              const std::string& name,
              const std::vector<net::CanonicalCookie>& new_cookies,
              std::vector<net::CanonicalCookie>* out_removed_cookies,
              std::vector<net::CanonicalCookie>* out_added_cookies);

 private:
  // Compares two cookies, returning true if |lhs| comes before |rhs| in the
  // partial ordering defined for CookieSet. This effectively does a
  // lexicographic comparison of (domain, path, name) tuples for two cookies.
  struct CookieComparator {
    bool operator()(const net::CanonicalCookie& lhs,
                    const net::CanonicalCookie& rhs) const;
  };

  // Compares two cookies, returning true if |lhs| comes before |rhs| in the
  // partial ordering defined for CookieSet. This effectively does a
  // lexicographic comparison of (domain, path, name, value) tuples for two
  // cookies.
  struct CookieAndValueComparator {
    bool operator()(const net::CanonicalCookie& lhs,
                    const net::CanonicalCookie& rhs) const;
  };

  typedef std::set<net::CanonicalCookie, CookieComparator> CookieSet;
  typedef std::pair<GURL, std::string> CookieKey;
  typedef std::map<CookieKey, CookieSet> CookieKeyPathMap;

  CookieKeyPathMap cache_;
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_CACHE_H_
