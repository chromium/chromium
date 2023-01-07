// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/net/cookies/cookie_cache.h"

#include <algorithm>

#include "net/cookies/cookie_options.h"

namespace net {

CookieCache::CookieCache() {
}

CookieCache::~CookieCache() {
}

bool CookieCache::Update(const GURL& url,
                         const std::string& name,
                         const std::vector<net::CanonicalCookie>& new_cookies,
                         std::vector<net::CanonicalCookie>* out_removed_cookies,
                         std::vector<net::CanonicalCookie>* out_added_cookies) {
  CookieKey key(url, name);
  CookieSet old_set = cache_[key];
  CookieSet new_set(new_cookies.begin(), new_cookies.end());

  // Compute the changes and the removals.
  CookieSet added_cookies;
  CookieSet removed_cookies;
  std::set_difference(new_set.begin(), new_set.end(), old_set.begin(),
                      old_set.end(),
                      std::inserter(added_cookies, added_cookies.begin()),
                      CookieCache::CookieAndValueComparator());
  std::set_difference(old_set.begin(), old_set.end(), new_set.begin(),
                      new_set.end(),
                      std::inserter(removed_cookies, removed_cookies.begin()),
                      CookieCache::CookieAndValueComparator());

  if (added_cookies.empty() && removed_cookies.empty())
    return false;

  cache_[key] = new_set;
  if (out_removed_cookies) {
    out_removed_cookies->insert(out_removed_cookies->end(),
                                removed_cookies.begin(), removed_cookies.end());
  }
  if (out_added_cookies) {
    out_added_cookies->insert(out_added_cookies->end(), added_cookies.begin(),
                              added_cookies.end());
  }
  return true;
}

bool CookieCache::CookieComparator::operator()(
    const net::CanonicalCookie& lhs,
    const net::CanonicalCookie& rhs) const {
  if (lhs.Domain() != rhs.Domain())
    return lhs.Domain() < rhs.Domain();
  if (lhs.Path() != rhs.Path())
    return lhs.Path() < rhs.Path();
  if (lhs.Name() != rhs.Name())
    return lhs.Name() < rhs.Name();
  return false;
}

bool CookieCache::CookieAndValueComparator::operator()(
    const net::CanonicalCookie& lhs,
    const net::CanonicalCookie& rhs) const {
  if (lhs.Domain() != rhs.Domain())
    return lhs.Domain() < rhs.Domain();
  if (lhs.Path() != rhs.Path())
    return lhs.Path() < rhs.Path();
  if (lhs.Name() != rhs.Name())
    return lhs.Name() < rhs.Name();
  if (lhs.Value() != rhs.Value())
    return lhs.Value() < rhs.Value();
  return false;
}

}  // namespace net
