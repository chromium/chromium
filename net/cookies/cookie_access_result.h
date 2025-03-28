// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_ACCESS_RESULT_H_
#define NET_COOKIES_COOKIE_ACCESS_RESULT_H_

#include <ostream>

#include "net/base/net_export.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"

namespace net {

struct NET_EXPORT CookieAccessResult {
  // Creating a CookieAccessResult with out any parameters will create a
  // CookieInclusionStatus that has no exclusion reasons, therefore
  // indicates inclusion.
  CookieAccessResult();
  CookieAccessResult(CookieEffectiveSameSite effective_same_site,
                     CookieInclusionStatus status,
                     CookieAccessSemantics access_semantics,
                     CookieScopeSemantics scope_semantics,
                     bool is_allowed_to_access_secure_cookie);

  explicit CookieAccessResult(CookieInclusionStatus status);

  CookieAccessResult(const CookieAccessResult& cookie_access_result);

  CookieAccessResult& operator=(const CookieAccessResult& cookie_access_result);

  CookieAccessResult(CookieAccessResult&& cookie_access_result);

  ~CookieAccessResult();

  friend bool operator==(const CookieAccessResult&,
                         const CookieAccessResult&) = default;

  CookieInclusionStatus status;
  CookieEffectiveSameSite effective_same_site =
      CookieEffectiveSameSite::UNDEFINED;
  CookieAccessSemantics access_semantics = CookieAccessSemantics::UNKNOWN;
  CookieScopeSemantics scope_semantics = CookieScopeSemantics::UNKNOWN;
  // Whether access to Secure cookies should be allowed. This is expected to be
  // set based on the scheme of the source URL.
  bool is_allowed_to_access_secure_cookies = false;
};

// Provided to allow gtest to create more helpful error messages, instead of
// printing hex.
inline void PrintTo(const CookieAccessResult& car, std::ostream* os) {
  *os << "{ { ";
  PrintTo(car.status, os);
  *os << " }, effective_same_site=" << static_cast<int>(car.effective_same_site)
      << ", access_semantics=" << static_cast<int>(car.access_semantics)
      << ", scope_semantics=" << static_cast<int>(car.scope_semantics)
      << ", is_allowed_to_access_secure_cookies="
      << car.is_allowed_to_access_secure_cookies << " }";
}

}  // namespace net

#endif  // NET_COOKIES_COOKIE_ACCESS_RESULT_H_
